#pragma once
#include <stdint.h>
#include <stdlib.h> // Requerido para la función estándar free()

struct Texture {
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t* pixels = nullptr; // Búfer RGB565 asignado con malloc/heap_caps_malloc
    uint8_t* alpha = nullptr;   // Canal de opacidad (0-255) asignado con malloc/heap_caps_malloc

    ~Texture() {
        // SOLUCIÓN: Usar free() para liberar memoria asignada dinámicamente con malloc
        if (pixels) {
            free(pixels);
            pixels = nullptr;
        }
        if (alpha) {
            free(alpha);
            alpha = nullptr;
        }
    }
};
