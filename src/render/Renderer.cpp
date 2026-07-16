// src/render/Renderer.cpp
#include "Renderer.h"
#include "Blend.h"
#include <string.h>
#include <Arduino.h>

extern bool debugMode;

Renderer::Renderer(uint16_t w, uint16_t h) 
    : width(w), height(h), framebuffer(nullptr), backgroundBuffer(nullptr),
      petBackingStore(nullptr), petX(0), petY(0), petW(0), petH(0), petBackingStoreSaved(false) {
    
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
    if (petBackingStore) free(petBackingStore);
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
    size_t cropSize = petW * petH * sizeof(uint16_t);

    if (petBackingStore) free(petBackingStore);
    petBackingStore = (uint16_t*)malloc(cropSize);

    if (petBackingStore && framebuffer) {
        for (int16_t y = 0; y < petH; ++y) {
            for (int16_t x = 0; x < petW; ++x) {
                int16_t fbX = petX + x;
                int16_t fbY = petY + y;
                if (fbX >= 0 && fbX < width && fbY >= 0 && fbY < height) {
                    petBackingStore[y * petW + x] = framebuffer[fbY * width + fbX];
                }
            }
        }
        petBackingStoreSaved = true;
        Serial.println("[RENDERER] Backing Store de mascota guardado de forma segura en SRAM (41 KB).");
    }
}

void Renderer::render(const std::vector<std::shared_ptr<RenderObject>>& objects) {
    if (!framebuffer) return;

    if (backgroundBuffer) {
        // MODO BUFFER ESTÁTICO (PSRAM)
        memcpy(framebuffer, backgroundBuffer, width * height * sizeof(uint16_t));
    } else {
        // MODO SRAM OPTIMIZADO (Sin PSRAM)
        if (petBackingStoreSaved && petBackingStore) {
            // Restaurar los píxeles de fondo originales debajo de la mascota antes de dibujar el frame
            for (int16_t y = 0; y < petH; ++y) {
                int16_t fbY = petY + y;
                if (fbY < 0 || fbY >= height) continue;
                for (int16_t x = 0; x < petW; ++x) {
                    int16_t fbX = petX + x;
                    if (fbX < 0 || fbX >= width) continue;
                    framebuffer[fbY * width + fbX] = petBackingStore[y * petW + x];
                }
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
        uint32_t freeHeap = ESP.getFreeHeap();
        int32_t barW = (freeHeap * width) / (200 * 1024);
        if (barW > width) barW = width;
        if (barW < 0) barW = 0;

        uint16_t barColor = 0x07E0; // Verde (> 100 KB)
        if (freeHeap < 50 * 1024) {
            barColor = 0xF800; // Rojo (< 50 KB)
        } else if (freeHeap < 100 * 1024) {
            barColor = 0xFEE0; // Amarillo (< 100 KB)
        }

        for (int16_t y = 0; y < 6; ++y) {
            for (int16_t x = 0; x < width; ++x) {
                if (x < barW) {
                    framebuffer[y * width + x] = barColor;
                } else {
                    framebuffer[y * width + x] = 0x3186;
                }
            }
        }
    }
}
