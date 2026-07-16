#pragma once
#include <stdint.h>

/**
 * Realiza una mezcla alfa (Alpha Blending) rápida de dos colores RGB565.
 * @param bg Color de fondo (RGB565)
 * @param fg Color de frente (RGB565)
 * @param alpha Nivel de opacidad (0 = completamente transparente, 255 = opaco)
 */
inline uint16_t blendRGB565(uint16_t bg, uint16_t fg, uint8_t alpha) {
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;

    // Extracción de canales
    uint32_t r_bg = (bg >> 11) & 0x1F;
    uint32_t g_bg = (bg >> 5) & 0x3F;
    uint32_t b_bg = bg & 0x1F;

    uint32_t r_fg = (fg >> 11) & 0x1F;
    uint32_t g_fg = (fg >> 5) & 0x3F;
    uint32_t b_fg = fg & 0x1F;

    // Mezcla lineal interpolada
    uint32_t r_blended = ((r_fg * alpha) + (r_bg * (255 - alpha))) >> 8;
    uint32_t g_blended = ((g_fg * alpha) + (g_bg * (255 - alpha))) >> 8;
    uint32_t b_blended = ((b_fg * alpha) + (b_bg * (255 - alpha))) >> 8;

    return (r_blended << 11) | (g_blended << 5) | b_blended;
}
