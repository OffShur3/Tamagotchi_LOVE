#pragma once
#include <stdint.h>

struct Vector2 {
    int16_t x = 0;
    int16_t y = 0;
};

struct Size2 {
    int16_t w = 0;
    int16_t h = 0;
};

class RenderContext {
public:
    RenderContext(uint16_t w, uint16_t h, uint16_t* buffer) 
        : width(w), height(h), framebuffer(buffer) {}
    
    const uint16_t width;
    const uint16_t height;
    uint16_t* const framebuffer;

    inline void drawPixel(int16_t x, int16_t y, uint16_t color) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            framebuffer[y * width + x] = color;
        }
    }
};
