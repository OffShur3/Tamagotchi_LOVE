// src/assets/AssetManager.cpp
#include "AssetManager.h"
#include "../ColorTuner.h"
#include <Arduino.h>
#include <math.h> // Para senos y cosenos

struct PNGUserContext {
    uint16_t* dest_pixels;
    uint8_t* dest_alpha;
    uint16_t width;
    PNG* png;
    ColorDomain domain;
    uint32_t dnaSeed; // Inyectar semilla al decodificador
};

// Algoritmo de rotación de matiz (Visual DNA)
static void applyVisualDNA(uint8_t& r, uint8_t& g, uint8_t& b, uint32_t seed) {
    if (seed == 0) return;
    
    // Proteger colores neutros: contornos (negros), blanco puro y grises puros
    if ((r < 40 && g < 40 && b < 40) || (r > 220 && g > 220 && b > 220)) return;
    if (abs(r - g) < 15 && abs(g - b) < 15 && abs(r - b) < 15) return;

    // Extraer ángulo pseudorandom de la semilla
    float angle = (seed % 360) * (PI / 180.0f);
    float cosA = cos(angle);
    float sinA = sin(angle);

    // Matriz de preservación de luminancia Luma rápida
    float matrix[3][3] = {
        { cosA + (1.0f - cosA) / 3.0f, 1.0f/3.0f * (1.0f - cosA) - sqrtf(1.0f/3.0f) * sinA, 1.0f/3.0f * (1.0f - cosA) + sqrtf(1.0f/3.0f) * sinA },
        { 1.0f/3.0f * (1.0f - cosA) + sqrtf(1.0f/3.0f) * sinA, cosA + 1.0f/3.0f * (1.0f - cosA), 1.0f/3.0f * (1.0f - cosA) - sqrtf(1.0f/3.0f) * sinA },
        { 1.0f/3.0f * (1.0f - cosA) - sqrtf(1.0f/3.0f) * sinA, 1.0f/3.0f * (1.0f - cosA) + sqrtf(1.0f/3.0f) * sinA, cosA + 1.0f/3.0f * (1.0f - cosA) }
    };

    float newR = r * matrix[0][0] + g * matrix[0][1] + b * matrix[0][2];
    float newG = r * matrix[1][0] + g * matrix[1][1] + b * matrix[1][2];
    float newB = r * matrix[2][0] + g * matrix[2][1] + b * matrix[2][2];

    r = (uint8_t)constrain(newR, 0.0f, 255.0f);
    g = (uint8_t)constrain(newG, 0.0f, 255.0f);
    b = (uint8_t)constrain(newB, 0.0f, 255.0f);
}

// Deduce el dominio de renderizado a partir del path del asset
static ColorDomain deduceDomain(const std::string& path) {
    if (path.find("sprites/base/") != std::string::npos) {
        return DOMAIN_TAMA;
    }
    if (path.find("icons_ui.png") != std::string::npos) {
        return DOMAIN_BUTTONS;
    }
    if (path.find("bg_main.png") != std::string::npos || path.find("objects_env.png") != std::string::npos) {
        return DOMAIN_WORLD;
    }
    return DOMAIN_WORLD;
}

int PNGDrawCallback(PNGDRAW *pDraw) {
    PNGUserContext* ctx = (PNGUserContext*)pDraw->pUser;
    uint16_t* dest_row = ctx->dest_pixels + (pDraw->y * ctx->width);
    uint8_t* dest_alpha_row = ctx->dest_alpha + (pDraw->y * ctx->width);

    ColorDomain domain = ctx->domain;
    bool hasTuning = colorTunerHasEffect(domain);

    if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
        uint8_t* src = (uint8_t*)pDraw->pPixels;
        for (int x = 0; x < pDraw->iWidth; ++x) {
            uint8_t r = src[0];
            uint8_t g = src[1];
            uint8_t b = src[2];
            uint8_t a = src[3];
            src += 4;

            // Aplicar DNA si es una textura de Mascota
            if (domain == DOMAIN_TAMA && ctx->dnaSeed != 0) {
                applyVisualDNA(r, g, b, ctx->dnaSeed);
            }

            if (hasTuning) {
                dest_row[x] = colorTunerApply(domain, r, g, b);
            } else {
                dest_row[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            }

            if (ctx->dest_alpha) {
                dest_alpha_row[x] = a;
            }
        }
    } else {
        ctx->png->getLineAsRGB565(pDraw, dest_row, PNG_RGB565_LITTLE_ENDIAN, 0);

        for (int x = 0; x < pDraw->iWidth; ++x) {
            uint16_t c565 = dest_row[x];
            
            // Expandir, mutar y contraer si es Indexado/565 nativo
            if (domain == DOMAIN_TAMA && ctx->dnaSeed != 0) {
                uint8_t r = ((c565 >> 11) & 0x1F) * 255 / 31;
                uint8_t g = ((c565 >> 5) & 0x3F) * 255 / 63;
                uint8_t b = (c565 & 0x1F) * 255 / 31;
                applyVisualDNA(r, g, b, ctx->dnaSeed);
                c565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            }

            if (hasTuning) {
                c565 = colorTunerCorrect565(domain, c565);
            }
            dest_row[x] = c565;
        }

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

void PNGCloseCallback(void *handle) {}

int32_t PNGReadCallback(PNGFILE *handle, uint8_t *buffer, int32_t length) {
    fs::File* f = (fs::File*)handle->fHandle;
    return f->read(buffer, length);
}

int32_t PNGSeekCallback(PNGFILE *handle, int32_t position) {
    fs::File* f = (fs::File*)handle->fHandle;
    return f->seek(position);
}

std::shared_ptr<Texture> AssetManager::getTexture(const std::string& filepath) {
    std::string cacheKey = filepath;
    
    // Evitar colisiones en el cache anexando el DNA a la clave del mapa
    if (activeDnaSeed != 0 && deduceDomain(filepath) == DOMAIN_TAMA) {
        char seedStr[16];
        snprintf(seedStr, sizeof(seedStr), "_%08X", activeDnaSeed);
        cacheKey += seedStr;
    }

    auto it = cache.find(cacheKey);
    if (it != cache.end()) {
        return it->second;
    }

    auto texture = loadFromFile(filepath);
    if (texture) {
        cache[cacheKey] = texture; // Guardar bajo la clave mutada
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

    Serial.println("\n--------------------------------------------------");
    Serial.printf("[ASSETS] Abriendo archivo: %s\n", path.c_str());
    Serial.printf("[ASSETS] Dominio asignado: %s\n", getColorDomainName(deduceDomain(path)));
    Serial.printf("[ASSETS] Dimensiones:      %dx%d píxeles\n", tex->width, tex->height);
    Serial.printf("[ASSETS] Memoria requerida: %u Bytes (%u KB)\n", 
                  (unsigned)(colorBytes + alphaBytes), (unsigned)((colorBytes + alphaBytes) / 1024));
    Serial.printf("[ASSETS] Free PSRAM:       %u Bytes (%u KB)\n", 
                  ESP.getFreePsram(), ESP.getFreePsram() / 1024);
    Serial.println("--------------------------------------------------");

    tex->pixels = (uint16_t*)heap_caps_malloc(colorBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (hasAlpha) {
        tex->alpha = (uint8_t*)heap_caps_malloc(alphaBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    } else {
        tex->alpha = nullptr;
    }

    if (!tex->pixels || (hasAlpha && !tex->alpha)) {
        Serial.printf("[ASSETS] ERROR CRÍTICO: No hay suficiente PSRAM libre para %s\n", path.c_str());
        if (tex->pixels) heap_caps_free(tex->pixels);
        if (tex->alpha) heap_caps_free(tex->alpha);
        png->close();
        pngFile.close();
        return nullptr;
    }

    if (tex->alpha) {
        memset(tex->alpha, 255, alphaBytes);
    }

    // Inyectar la semilla al callback
    PNGUserContext ctx = { tex->pixels, tex->alpha, tex->width, png, deduceDomain(path), activeDnaSeed };
    rc = png->decode(&ctx, 0);
    
    png->close();
    pngFile.close();

    if (rc != PNG_SUCCESS) {
        Serial.printf("[ASSETS] Error decodificando píxeles del PNG: %s\n", path.c_str());
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

bool AssetManager::loadPNGDirectToBuffer(const std::string& path, uint16_t* destBuffer) {
    if (!fileSystem || !png || !destBuffer) return false;

    pngFile = fileSystem->open(path.c_str(), "r");
    if (!pngFile || pngFile.isDirectory()) return false;

    int rc = png->open(path.c_str(), PNGOpenCallback, PNGCloseCallback, PNGReadCallback, PNGSeekCallback, PNGDrawCallback);
    if (rc != PNG_SUCCESS) {
        pngFile.close();
        return false;
    }

        PNGUserContext ctx = { destBuffer, nullptr, (uint16_t)png->getWidth(), png, deduceDomain(path), 0 };
    rc = png->decode(&ctx, 0);
    
    png->close();
    pngFile.close();

    return rc == PNG_SUCCESS;
}
