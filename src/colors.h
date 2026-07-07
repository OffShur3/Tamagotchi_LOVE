#ifndef COLORS_H
#define COLORS_H

#include <stdint.h>

// Definición de colores en formato BGR (Blue-Green-Red) nativo para el ST7789
// Calculado como: (B & 0x1F) << 11 | (G & 0x3F) << 5 | (R & 0x1F)
#define COLOR_BLACK  0x0000
#define COLOR_WHITE  0xFFFF
#define COLOR_RED    0x001F
#define COLOR_GREEN  0x07E0
#define COLOR_BLUE   0xF800
#define COLOR_YELLOW 0x07FF

#endif
