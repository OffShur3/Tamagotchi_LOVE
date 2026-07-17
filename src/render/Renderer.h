#pragma once
#include <vector>
#include <memory>
#include <SD_MMC.h> // <--- AGREGAR ESTO
#include "RenderObject.h"
#include "../assets/Texture.h"

class Renderer {
public:
    Renderer(uint16_t width, uint16_t height);
    ~Renderer();

    void clear(uint16_t clearColor);
    void clearBackground(uint16_t clearColor);
    void drawToBackground(const std::shared_ptr<Texture>& tex, int16_t x, int16_t y);
    void drawToFramebufferOnce(const std::shared_ptr<Texture>& tex, int16_t x, int16_t y);
    void savePetBackingStore(int16_t px, int16_t py, int16_t pw, int16_t ph);
    void render(const std::vector<std::shared_ptr<RenderObject>>& objects);
    
    bool hasBackgroundBuffer() const { return backgroundBuffer != nullptr; }
    uint16_t* getFramebuffer() { return framebuffer; }
    const uint16_t* getFramebuffer() const { return framebuffer; }
    uint16_t getWidth() const { return width; }
    uint16_t getHeight() const { return height; }

private:
    uint16_t width;
    uint16_t height;
    uint16_t* framebuffer;
    uint16_t* backgroundBuffer; 

    // --- NUEVO SISTEMA DE BACKING STORE EN SD ---
    File bgFile; 
    int16_t petX, petY, petW, petH;
    bool petBackingStoreSaved;
};
