// src/render/Renderer.h
#pragma once
#include <vector>
#include <memory>
#include "RenderObject.h"
#include "../assets/Texture.h"

class Renderer {
public:
    Renderer(uint16_t width, uint16_t height);
    ~Renderer();

    void clear(uint16_t clearColor);
    
    // Métodos para modo PSRAM
    void clearBackground(uint16_t clearColor);
    void drawToBackground(const std::shared_ptr<Texture>& tex, int16_t x, int16_t y);
    
    // Métodos para modo SRAM optimizado (Sin PSRAM)
    void drawToFramebufferOnce(const std::shared_ptr<Texture>& tex, int16_t x, int16_t y);
    void savePetBackingStore(int16_t px, int16_t py, int16_t pw, int16_t ph);
    
    void render(const std::vector<std::shared_ptr<RenderObject>>& objects);
    
    bool hasBackgroundBuffer() const { return backgroundBuffer != nullptr; }
    const uint16_t* getFramebuffer() const { return framebuffer; }
    uint16_t getWidth() const { return width; }
    uint16_t getHeight() const { return height; }

private:
    uint16_t width;
    uint16_t height;
    uint16_t* framebuffer;
    uint16_t* backgroundBuffer; // Búfer estático de PSRAM

    // Variables de Backing Store de Mascota en SRAM interna
    uint16_t* petBackingStore;
    int16_t petX, petY, petW, petH;
    bool petBackingStoreSaved;
};
