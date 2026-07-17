#pragma once
#include <stdint.h>

struct Vector2 {
    int16_t x;
    int16_t y;

    // Constructores explícitos para compatibilidad con C++11
    Vector2() : x(0), y(0) {}
    Vector2(int16_t _x, int16_t _y) : x(_x), y(_y) {}
};

struct Size2 {
    int16_t w;
    int16_t h;

    // Constructores explícitos para compatibilidad con C++11
    Size2() : w(0), h(0) {}
    Size2(int16_t _w, int16_t _h) : w(_w), h(_h) {}
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
