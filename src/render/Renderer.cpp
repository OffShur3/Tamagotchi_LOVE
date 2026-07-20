// src/render/Renderer.cpp
#include "Renderer.h"
#include "Blend.h"
#include <string.h>
#include <Arduino.h>

extern bool debugMode;

Renderer::Renderer(uint16_t w, uint16_t h) 
    : width(w), height(h), framebuffer(nullptr), backgroundBuffer(nullptr),
      petX(0), petY(0), petW(0), petH(0), petBackingStoreSaved(false) { 
    
    size_t size = width * height * sizeof(uint16_t);

    framebuffer = (uint16_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!framebuffer) {
        framebuffer = (uint16_t*)malloc(size);
    }

    backgroundBuffer = (uint16_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    
    if (framebuffer) {
        memset(framebuffer, 0, size);
    } else {
        Serial.println("[RENDERER] Error crítico: No hay suficiente memoria para el framebuffer de trabajo.");
    }

    if (backgroundBuffer) {
        memset(backgroundBuffer, 0, size);
        Serial.println("[RENDERER] PSRAM detectada: Búfer de fondo estático activado.");
    } else {
        Serial.println("[RENDERER] No hay PSRAM física: Ejecutando en modo SRAM optimizado (búfer de fondo desactivado).");
    }
}

Renderer::~Renderer() {
    if (framebuffer) free(framebuffer);
    if (backgroundBuffer) free(backgroundBuffer);
    if (bgFile) bgFile.close(); // <--- CERRAR ARCHIVO EN LUGAR DE LIBERAR RAM
}

void Renderer::clear(uint16_t clearColor) {
    if (!framebuffer) return;
    for (uint32_t i = 0; i < (uint32_t)(width * height); ++i) {
        framebuffer[i] = clearColor;
    }
}

void Renderer::clearBackground(uint16_t clearColor) {
    if (!backgroundBuffer) return;
    for (uint32_t i = 0; i < (uint32_t)(width * height); ++i) {
        backgroundBuffer[i] = clearColor;
    }
}

void Renderer::drawToBackground(const std::shared_ptr<Texture>& tex, int16_t x, int16_t y) {
    if (!tex || !tex->pixels || !backgroundBuffer) return;

    int16_t texW = tex->width;
    int16_t texH = tex->height;

    int16_t scaleX = (texW < width) ? 2 : 1;
    int16_t scaleY = (texH < height) ? 2 : 1;

    for (int16_t j = 0; j < texH; j++) {
        int16_t destY = y + j * scaleY;
        for (int16_t sy = 0; sy < scaleY; ++sy) {
            int16_t finalY = destY + sy;
            if (finalY < 0 || finalY >= height) continue;

            for (int16_t i = 0; i < texW; i++) {
                int16_t destX = x + i * scaleX;
                
                size_t srcIdx = (size_t)j * texW + i;
                uint16_t color = tex->pixels[srcIdx];
                uint8_t alpha = tex->alpha ? tex->alpha[srcIdx] : 255;

                if (alpha == 0) continue;

                for (int16_t sx = 0; sx < scaleX; ++sx) {
                    int16_t finalX = destX + sx;
                    if (finalX < 0 || finalX >= width) continue;

                    size_t dstIdx = (size_t)finalY * width + finalX;
                    if (alpha == 255) {
                        backgroundBuffer[dstIdx] = color;
                    } else {
                        backgroundBuffer[dstIdx] = blendRGB565(backgroundBuffer[dstIdx], color, alpha);
                    }
                }
            }
        }
    }
}

void Renderer::drawToFramebufferOnce(const std::shared_ptr<Texture>& tex, int16_t x, int16_t y) {
    if (!tex || !tex->pixels || !framebuffer) return;

    int16_t texW = tex->width;
    int16_t texH = tex->height;

    int16_t scaleX = (texW < width) ? 2 : 1;
    int16_t scaleY = (texH < height) ? 2 : 1;

    for (int16_t j = 0; j < texH; j++) {
        int16_t destY = y + j * scaleY;
        for (int16_t sy = 0; sy < scaleY; ++sy) {
            int16_t finalY = destY + sy;
            if (finalY < 0 || finalY >= height) continue;

            for (int16_t i = 0; i < texW; i++) {
                int16_t destX = x + i * scaleX;
                
                size_t srcIdx = (size_t)j * texW + i;
                uint16_t color = tex->pixels[srcIdx];

                for (int16_t sx = 0; sx < scaleX; ++sx) {
                    int16_t finalX = destX + sx;
                    if (finalX < 0 || finalX >= width) continue;

                    framebuffer[finalY * width + finalX] = color;
                }
            }
        }
    }
}

void Renderer::savePetBackingStore(int16_t px, int16_t py, int16_t pw, int16_t ph) {
    petX = px; petY = py; petW = pw; petH = ph;

    // Crear archivo binario en modo lectura/escritura (w+)
    bgFile = SD_MMC.open("/tama/tmp_bg.bin", "w+");
    
    if (bgFile && framebuffer) {
        for (int16_t y = 0; y < petH; ++y) {
            int16_t fbY = petY + y;
            if (fbY >= 0 && fbY < height) {
                // Escribir la fila cruda directo del framebuffer a la SD
                bgFile.write((uint8_t*)&framebuffer[fbY * width + petX], petW * sizeof(uint16_t));
            }
        }
        bgFile.flush();
        petBackingStoreSaved = true;
        Serial.println("[RENDERER] Backing Store delegado a SD (/tama/tmp_bg.bin). Ahorro masivo: 41 KB liberados.");
    }
}

void Renderer::render(const std::vector<std::shared_ptr<RenderObject>>& objects) {
    if (!framebuffer) return;

    if (backgroundBuffer) {
        // MODO BUFFER ESTÁTICO (PSRAM)
        memcpy(framebuffer, backgroundBuffer, width * height * sizeof(uint16_t));
    } else {
        // MODO SRAM OPTIMIZADO (Sin PSRAM)
        if (petBackingStoreSaved && bgFile) {
            bgFile.seek(0); // Rebobinar el archivo de la SD
            for (int16_t y = 0; y < petH; ++y) {
                int16_t fbY = petY + y;
                if (fbY < 0 || fbY >= height) {
                    bgFile.seek(bgFile.position() + (petW * sizeof(uint16_t))); // Saltar fila
                    continue;
                }
                // Leer la fila desde la SD y escupirla en el Framebuffer
                bgFile.read((uint8_t*)&framebuffer[fbY * width + petX], petW * sizeof(uint16_t));
            }
        } else {
            // --- FALLBACK DE SEGURIDAD ---
            // Si el fondo de pantalla falló al cargarse, limpiamos la pantalla completa con gris sólido
            // para evitar estelas, fantasmas o apilamientos en las animaciones de la mascota.
            for (uint32_t i = 0; i < (uint32_t)(width * height); ++i) {
                framebuffer[i] = 0x18C3; 
            }
        }
    }

    RenderContext ctx(width, height, framebuffer);

    std::vector<std::shared_ptr<RenderObject>> layers[static_cast<int>(RenderLayer::Count)];

    for (const auto& obj : objects) {
        if (obj && obj->visible) {
            if (obj->layer == RenderLayer::Background) {
                continue;
            }
            layers[static_cast<int>(obj->layer)].push_back(obj);
        }
    }

    // Dibujar elementos dinámicos
    for (int l = 0; l < static_cast<int>(RenderLayer::Count); ++l) {
        for (const auto& obj : layers[l]) {
            obj->draw(ctx);
            obj->dirty = false;
        }
    }

    // --- HUD DE DIAGNÓSTICO EN TIEMPO REAL DIRECTO EN FRAMEBUFFER ---
    if (debugMode) {
        // 1. Obtener estados de memoria
        uint32_t totalSRAM = ESP.getHeapSize();
        uint32_t freeSRAM  = ESP.getFreeHeap();
        uint32_t usedSRAM  = totalSRAM - freeSRAM;

        uint32_t totalPSRAM = ESP.getPsramSize();
        uint32_t freePSRAM  = ESP.getFreePsram();
        uint32_t usedPSRAM  = totalPSRAM - freePSRAM;

        uint32_t totalMem = totalSRAM + totalPSRAM;

        if (totalMem > 0) {
            // 2. Calcular anchos proporcionales en los 172 píxeles de la pantalla
            int wSRAM = (totalSRAM * width) / totalMem;
            int wPSRAM = width - wSRAM; // El resto de la pantalla

            // 3. Calcular cuántos píxeles pintar de "usado" en cada sección
            int pxUsedSRAM = (usedSRAM * wSRAM) / totalSRAM;
            int pxUsedPSRAM = (totalPSRAM > 0) ? (usedPSRAM * wPSRAM) / totalPSRAM : 0;

            // 4. Paleta de colores Ciberpunk/Técnica
            uint16_t cSramUsed  = 0xF800; // Rojo (SRAM RAM Ocupada - CRÍTICO)
            uint16_t cSramFree  = 0x07E0; // Verde (SRAM RAM Libre - OK)
            uint16_t cPsramUsed = 0x981F; // Púrpura (PSRAM Ocupada)
            uint16_t cPsramFree = 0x07FF; // Cian / Azul Claro (PSRAM Libre)

            // 5. Dibujar la barra de 6 píxeles de alto en la parte superior
            for (int16_t y = 0; y < 6; ++y) {
                for (int16_t x = 0; x < width; ++x) {
                    if (x < wSRAM) {
                        // Pintando la porción de memoria interna (SRAM)
                        framebuffer[y * width + x] = (x < pxUsedSRAM) ? cSramUsed : cSramFree;
                    } else {
                        // Pintando la masiva porción de PSRAM externa
                        int localX = x - wSRAM;
                        framebuffer[y * width + x] = (localX < pxUsedPSRAM) ? cPsramUsed : cPsramFree;
                    }
                }
            }
        }
    }
}
