#ifndef COLORS_H
#define COLORS_H

#include <stdint.h>

// Función que recibe R, G, B (0-255) y devuelve el color en el formato
// que tu pantalla BGR entiende correctamente.
// Para pantalla en modo BGR: bits 15-11 = azul, 10-5 = verde, 4-0 = rojo.
static inline uint16_t colorBGR(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
}

// Redefinir colores básicos para que sean más vivos
#define COLOR_RED    colorBGR(255, 136, 0)
#define COLOR_GREEN  colorBGR(0, 255, 0)
#define COLOR_BLUE   colorBGR(255, 0, 0)
#define COLOR_WHITE  colorBGR(255, 255, 255)
#define COLOR_BLACK  colorBGR(0, 0, 0)
#define COLOR_YELLOW colorBGR(0, 255, 255)

// ---- Paleta Material Design ----
#define MAT_BG       colorBGR(16, 8, 16) // Gris muy oscuro pero no negro total
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

// ---- Colores para el juego Tamagotchi ----
#define TAMA_BROWN colorBGR(76, 40, 24) // #4C2818
#define TAMA_UI_BG colorBGR(245, 223, 191) // Un crema tipo Stardew

#endif
