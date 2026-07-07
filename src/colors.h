#ifndef COLORS_H
#define COLORS_H

#include <stdint.h>

#define BGR_RED    0x001F
#define BGR_GREEN  0x07E0
#define BGR_BLUE   0xF800
#define BGR_WHITE  0xFFFF
#define BGR_BLACK  0x0000
#define BGR_YELLOW 0x07FF

#define RGB565_BGR(r, g, b) (((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3))

#define MAT_BG       RGB565_BGR(18, 18, 18)   
#define MAT_OFFLINE  RGB565_BGR(66, 66, 66)   
#define MAT_CONNECT  RGB565_BGR(67, 160, 71)  

#endif
