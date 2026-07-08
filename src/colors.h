#ifndef COLORS_H
#define COLORS_H

#include <stdint.h>

// Función que recibe R, G, B (0-255) y devuelve el color en el formato
// que tu pantalla BGR entiende correctamente.
// Para pantalla en modo BGR: bits 15-11 = azul, 10-5 = verde, 4-0 = rojo.
static inline uint16_t colorBGR(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(b & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (r >> 3);
}

// ---- Colores básicos (usa colorBGR para garantizar tonos correctos) ----
#define COLOR_RED    colorBGR(255, 0, 0)    // Rojo real
#define COLOR_GREEN  colorBGR(0, 255, 0)    // Verde real
#define COLOR_BLUE   colorBGR(0, 0, 255)    // Azul real
#define COLOR_WHITE  colorBGR(255, 255, 255)
#define COLOR_BLACK  colorBGR(0, 0, 0)
#define COLOR_YELLOW colorBGR(255, 255, 0)   // Amarillo real

// ---- Paleta Material Design ----
#define MAT_BG       colorBGR(18, 18, 18)    // Fondo oscuro
#define MAT_OFFLINE  colorBGR(66, 66, 66)    // Botón secundario
#define MAT_CONNECT  colorBGR(67, 160, 71)   // Botón de confirmación

// (Opcional) conserva los antiguos nombres para compatibilidad,
// pero siempre con la conversión correcta.
#define BGR_RED    COLOR_RED
#define BGR_GREEN  COLOR_GREEN
#define BGR_BLUE   COLOR_BLUE
#define BGR_WHITE  COLOR_WHITE
#define BGR_BLACK  COLOR_BLACK
#define BGR_YELLOW COLOR_YELLOW

#endif
