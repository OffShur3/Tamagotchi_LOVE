// src/assets/AssetManager.cpp
#include "AssetManager.h"
#include <Arduino.h>

// Contexto de dibujo enriquecido con el puntero al decodificador
struct PNGUserContext {
    uint16_t* dest_pixels;
    uint8_t* dest_alpha;
    uint16_t width;
    PNG* png; // Permite al callback invocar métodos de conversión interna de la librería
};

int PNGDrawCallback(PNGDRAW *pDraw) {
    PNGUserContext* ctx = (PNGUserContext*)pDraw->pUser;
    uint16_t* dest_row = ctx->dest_pixels + (pDraw->y * ctx->width);
    uint8_t* dest_alpha_row = ctx->dest_alpha + (pDraw->y * ctx->width);

    // Si la imagen es un PNG con canal Alfa nativo de 32 bits (RGBA)
    if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
        uint8_t* src = (uint8_t*)pDraw->pPixels;
        for (int x = 0; x < pDraw->iWidth; ++x) {
            uint8_t r = src[0];
            uint8_t g = src[1];
            uint8_t b = src[2];
            uint8_t a = src[3];
            src += 4;

            dest_row[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            if (ctx->dest_alpha) {
                dest_alpha_row[x] = a;
            }
        }
    } else {
        // Decodificación de colores segura para indexados, escala de grises y RGB plano sin alfa
        ctx->png->getLineAsRGB565(pDraw, dest_row, PNG_RGB565_LITTLE_ENDIAN, 0);

        // Si la imagen no tiene canal alfa nativo, la marcamos como 100% opaca por defecto
        if (ctx->dest_alpha) {
            memset(dest_alpha_row, 255, pDraw->iWidth);
        }
    }
    return 1;
}

namespace {
    fs::File pngFile;
}

void* PNGOpenCallback(const char *filename, int32_t *size) {
    *size = pngFile.size();
    return &pngFile;
}

void PNGCloseCallback(void *handle) {
    // Se cierra de manera explícita en loadFromFile
}

int32_t PNGReadCallback(PNGFILE *handle, uint8_t *buffer, int32_t length) {
    fs::File* f = (fs::File*)handle->fHandle;
    return f->read(buffer, length);
}

int32_t PNGSeekCallback(PNGFILE *handle, int32_t position) {
    fs::File* f = (fs::File*)handle->fHandle;
    return f->seek(position);
}

std::shared_ptr<Texture> AssetManager::getTexture(const std::string& filepath) {
    auto it = cache.find(filepath);
    if (it != cache.end()) {
        return it->second;
    }

    auto texture = loadFromFile(filepath);
    if (texture) {
        cache[filepath] = texture;
    }
    return texture;
}

std::shared_ptr<Texture> AssetManager::loadFromFile(const std::string& path) {
    if (!fileSystem || !png) return nullptr;

    pngFile = fileSystem->open(path.c_str(), "r");
    if (!pngFile || pngFile.isDirectory()) {
        Serial.printf("[ASSETS] No se pudo abrir el archivo %s\n", path.c_str());
        return nullptr;
    }

    int rc = png->open(path.c_str(), PNGOpenCallback, PNGCloseCallback, PNGReadCallback, PNGSeekCallback, PNGDrawCallback);
    if (rc != PNG_SUCCESS) {
        Serial.printf("[ASSETS] Error abriendo estructura PNG: %s\n", path.c_str());
        pngFile.close();
        return nullptr;
    }

    auto tex = std::make_shared<Texture>();
    tex->width = png->getWidth();
    tex->height = png->getHeight();
    bool hasAlpha = png->hasAlpha() || (png->getPixelType() == PNG_PIXEL_TRUECOLOR_ALPHA);

    size_t colorBytes = (size_t)tex->width * tex->height * sizeof(uint16_t);
    size_t alphaBytes = hasAlpha ? (size_t)tex->width * tex->height : 0;

    // --- TELEMETRÍA DE SEGURIDAD EN ARRANQUE POR PUERTO SERIAL ---
    Serial.println("\n--------------------------------------------------");
    Serial.printf("[ASSETS] Abriendo archivo: %s\n", path.c_str());
    Serial.printf("[ASSETS] Dimensiones:      %dx%d píxeles\n", tex->width, tex->height);
    Serial.printf("[ASSETS] Memoria requerida: %u Bytes (%u KB)\n", 
                  (unsigned)(colorBytes + alphaBytes), (unsigned)((colorBytes + alphaBytes) / 1024));
    Serial.printf("[ASSETS] Bloque contiguo más grande en Heap: %u Bytes (%u KB)\n", 
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
    Serial.println("--------------------------------------------------");

    // Intentar alojar buffers en PSRAM; fallback automático a RAM interna
    tex->pixels = (uint16_t*)heap_caps_malloc(colorBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!tex->pixels) {
        tex->pixels = (uint16_t*)malloc(colorBytes);
    }

    if (hasAlpha) {
        tex->alpha = (uint8_t*)heap_caps_malloc(alphaBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!tex->alpha) {
            tex->alpha = (uint8_t*)malloc(alphaBytes);
        }
    } else {
        tex->alpha = nullptr;
    }

    if (!tex->pixels || (hasAlpha && !tex->alpha)) {
        Serial.printf("[ASSETS] ERROR: Memoria insuficiente para alojar %s en SRAM.\n", path.c_str());
        png->close();
        pngFile.close();
        return nullptr;
    }

    // Inicializar el alfa a 255 (totalmente opaco)
    if (tex->alpha) {
        memset(tex->alpha, 255, alphaBytes);
    }

    // Inyectamos la referencia del decodificador 'png' al contexto
    PNGUserContext ctx = { tex->pixels, tex->alpha, tex->width, png };
    rc = png->decode(&ctx, 0);
    
    png->close();
    pngFile.close();

    if (rc != PNG_SUCCESS) {
        Serial.printf("[ASSETS] Error decodificando pixeles del archivo PNG: %s\n", path.c_str());
        return nullptr; 
    }

    return tex;
}

void AssetManager::clearUnused() {
    for (auto it = cache.begin(); it != cache.end(); ) {
        if (it->second.use_count() <= 1) {
            it = cache.erase(it);
        } else {
            ++it;
        }
    }
}
// src/assets/AssetManager.cpp (Agregar al final del archivo)
bool AssetManager::loadPNGDirectToBuffer(const std::string& path, uint16_t* destBuffer) {
    if (!fileSystem || !png || !destBuffer) return false;

    pngFile = fileSystem->open(path.c_str(), "r");
    if (!pngFile || pngFile.isDirectory()) return false;

    int rc = png->open(path.c_str(), PNGOpenCallback, PNGCloseCallback, PNGReadCallback, PNGSeekCallback, PNGDrawCallback);
    if (rc != PNG_SUCCESS) {
        pngFile.close();
        return false;
    }

    // Inyectamos la dirección de la pantalla directamente. Cero bytes extra de RAM.
    PNGUserContext ctx = { destBuffer, nullptr, (uint16_t)png->getWidth(), png };
    rc = png->decode(&ctx, 0);
    
    png->close();
    pngFile.close();

    return rc == PNG_SUCCESS;
}
