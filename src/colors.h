#ifndef COLORS_H
#define COLORS_H

#include <stdint.h>

// Conversor de RGB a BGR
#define RGB565_BGR(r, g, b) (((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3))

// Base Colour
#define BGR_RED    RGB565_BGR(255, 0, 0)
#define BGR_GREEN  RGB565_BGR(0, 255, 0)
#define BGR_BLUE   RGB565_BGR(0, 0, 255)
#define BGR_WHITE  RGB565_BGR(255, 255, 255)
#define BGR_BLACK  RGB565_BGR(0, 0, 0)
#define BGR_YELLOW RGB565_BGR(255, 255, 0)

// Material Design Colours
#define MAT_BG       RGB565_BGR(18, 18, 18)   
#define MAT_OFFLINE  RGB565_BGR(66, 66, 66)   
#define MAT_CONNECT  RGB565_BGR(67, 160, 71)  

#endif
