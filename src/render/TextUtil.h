// src/render/TextUtil.h
#pragma once
#include <stdint.h>
#include <string.h>
#include "RenderContext.h"
#include "Blend.h"

class TextUtil {
public:
    static const uint8_t FONT_CHAR_W = 5;
    static const uint8_t FONT_ADVANCE_X = 6;
    static const uint8_t FONT_LINE_H = 10;

    // Tabla completa ASCII 32 (' ') hasta 126 ('~') con soporte real de mayúsculas y minúsculas
    static inline const uint8_t* getGlyph(char c) {
        static const uint8_t font5x7[95][5] = {
            {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 ' '
            {0x00, 0x00, 0x5F, 0x00, 0x00}, // 33 '!'
            {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 '"'
            {0x14, 0x7F, 0x14, 0x7F, 0x14}, // 35 '#'
            {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // 36 '$'
            {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 '%'
            {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 '&'
            {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '''
            {0x00, 0x1C, 0x22, 0x41, 0x00}, // 40 '('
            {0x00, 0x41, 0x22, 0x1C, 0x00}, // 41 ')'
            {0x14, 0x08, 0x3E, 0x08, 0x14}, // 42 '*'
            {0x08, 0x08, 0x3E, 0x08, 0x08}, // 43 '+'
            {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ','
            {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 '-'
            {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 '.'
            {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 '/'
            {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 48 '0'
            {0x00, 0x42, 0x7F, 0x40, 0x00}, // 49 '1'
            {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 '2'
            {0x21, 0x41, 0x45, 0x4B, 0x31}, // 51 '3'
            {0x18, 0x14, 0x12, 0x7F, 0x10}, // 52 '4'
            {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 '5'
            {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 54 '6'
            {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 '7'
            {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 '8'
            {0x06, 0x49, 0x49, 0x29, 0x1E}, // 57 '9'
            {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 ':'
            {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ';'
            {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 '<'
            {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 '='
            {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 '>'
            {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 '?'
            {0x32, 0x49, 0x79, 0x41, 0x3E}, // 64 '@'
            {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 65 'A'
            {0x7F, 0x49, 0x49, 0x49, 0x36}, // 66 'B'
            {0x3E, 0x41, 0x41, 0x41, 0x22}, // 67 'C'
            {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 68 'D'
            {0x7F, 0x49, 0x49, 0x49, 0x41}, // 69 'E'
            {0x7F, 0x09, 0x09, 0x09, 0x01}, // 70 'F'
            {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 71 'G'
            {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 72 'H'
            {0x00, 0x41, 0x7F, 0x41, 0x00}, // 73 'I'
            {0x20, 0x40, 0x41, 0x3F, 0x01}, // 74 'J'
            {0x7F, 0x08, 0x14, 0x22, 0x41}, // 75 'K'
            {0x7F, 0x40, 0x40, 0x40, 0x40}, // 76 'L'
            {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 77 'M'
            {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 78 'N'
            {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 79 'O'
            {0x7F, 0x09, 0x09, 0x09, 0x06}, // 80 'P'
            {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 81 'Q'
            {0x7F, 0x09, 0x19, 0x29, 0x46}, // 82 'R'
            {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 'S'
            {0x01, 0x01, 0x7F, 0x01, 0x01}, // 84 'T'
            {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 85 'U'
            {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 86 'V'
            {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 87 'W'
            {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 'X'
            {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 'Y'
            {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 'Z'
            {0x00, 0x7F, 0x41, 0x41, 0x00}, // 91 '['
            {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 '\'
            {0x00, 0x41, 0x41, 0x7F, 0x00}, // 93 ']'
            {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 '^'
            {0x40, 0x40, 0x40, 0x40, 0x40}, // 95 '_'
            {0x00, 0x01, 0x02, 0x04, 0x00}, // 96 '`'
            {0x20, 0x54, 0x54, 0x54, 0x78}, // 97 'a'
            {0x7F, 0x48, 0x44, 0x44, 0x38}, // 98 'b'
            {0x38, 0x44, 0x44, 0x44, 0x20}, // 99 'c'
            {0x38, 0x44, 0x44, 0x48, 0x7F}, // 100 'd'
            {0x38, 0x54, 0x54, 0x54, 0x18}, // 101 'e'
            {0x08, 0x7E, 0x09, 0x01, 0x02}, // 102 'f'
            {0x0C, 0x52, 0x52, 0x52, 0x3E}, // 103 'g'
            {0x7F, 0x08, 0x04, 0x04, 0x78}, // 104 'h'
            {0x00, 0x44, 0x7D, 0x40, 0x00}, // 105 'i'
            {0x20, 0x40, 0x44, 0x3D, 0x00}, // 106 'j'
            {0x7F, 0x10, 0x28, 0x44, 0x00}, // 107 'k'
            {0x00, 0x41, 0x7F, 0x40, 0x00}, // 108 'l'
            {0x7C, 0x04, 0x18, 0x04, 0x78}, // 109 'm'
            {0x7C, 0x08, 0x04, 0x04, 0x78}, // 110 'n'
            {0x38, 0x44, 0x44, 0x44, 0x38}, // 111 'o'
            {0x7C, 0x14, 0x14, 0x14, 0x08}, // 112 'p'
            {0x08, 0x14, 0x14, 0x18, 0x7C}, // 113 'q'
            {0x7C, 0x08, 0x04, 0x04, 0x08}, // 114 'r'
            {0x48, 0x54, 0x54, 0x54, 0x20}, // 115 's'
            {0x04, 0x3F, 0x44, 0x40, 0x20}, // 116 't'
            {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 117 'u'
            {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 118 'v'
            {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 119 'w'
            {0x44, 0x28, 0x10, 0x28, 0x44}, // 120 'x'
            {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 121 'y'
            {0x44, 0x64, 0x54, 0x4C, 0x44}, // 122 'z'
            {0x00, 0x08, 0x36, 0x41, 0x00}, // 123 '{'
            {0x00, 0x00, 0x77, 0x00, 0x00}, // 124 '|'
            {0x00, 0x41, 0x36, 0x08, 0x00}, // 125 '}'
            {0x08, 0x08, 0x2A, 0x1C, 0x08}  // 126 '~'
        };

        if (c >= ' ' && c <= '~') {
            return font5x7[c - ' '];
        }
        return font5x7[0];
    }

    static inline void drawChar5x7(RenderContext& ctx, int16_t x, int16_t y, char c, uint16_t color, 
                                   int16_t clipYMin = 0, int16_t clipYMax = 320, uint8_t alpha = 255) {
        if (y + 7 < clipYMin || y > clipYMax) return;
        const uint8_t* line = getGlyph(c);

        for (int col = 0; col < 5; ++col) {
            int16_t px = x + col;
            if (px < 0 || px >= ctx.width) continue;
            uint8_t bits = line[col];
            for (int bit = 0; bit < 7; ++bit) {
                int16_t py = y + bit;
                if (py < clipYMin || py > clipYMax || py < 0 || py >= ctx.height) continue;
                if (bits & (1 << bit)) {
                    if (alpha >= 255) {
                        ctx.framebuffer[py * ctx.width + px] = color;
                    } else if (alpha > 0) {
                        uint16_t bg = ctx.framebuffer[py * ctx.width + px];
                        ctx.framebuffer[py * ctx.width + px] = blendRGB565(bg, color, alpha);
                    }
                }
            }
        }
    }

    // Modo tipográfico enriquecido 6x8 para diálogos de alta legibilidad
    static inline void drawChar6x8(RenderContext& ctx, int16_t x, int16_t y, char c, uint16_t color,
                                   int16_t clipYMin = 0, int16_t clipYMax = 320, uint8_t alpha = 255) {
        if (y + 8 < clipYMin || y > clipYMax) return;
        const uint8_t* line = getGlyph(c);

        for (int col = 0; col < 5; ++col) {
            uint8_t bits = line[col];
            for (int bit = 0; bit < 7; ++bit) {
                if (bits & (1 << bit)) {
                    int16_t py = y + bit;
                    if (py < clipYMin || py > clipYMax || py < 0 || py >= ctx.height) continue;
                    for (int bx = 0; bx <= 1; ++bx) {
                        int16_t px = x + col + bx;
                        if (px < 0 || px >= ctx.width) continue;
                        if (alpha >= 255) {
                            ctx.framebuffer[py * ctx.width + px] = color;
                        } else if (alpha > 0) {
                            uint16_t bg = ctx.framebuffer[py * ctx.width + px];
                            ctx.framebuffer[py * ctx.width + px] = blendRGB565(bg, color, alpha);
                        }
                    }
                }
            }
        }
    }

    static inline void drawCharScaled(RenderContext& ctx, int16_t x, int16_t y, char c, uint16_t color, 
                                      uint8_t scale, int16_t clipYMin = 0, int16_t clipYMax = 320, uint8_t alpha = 255) {
        if (scale <= 1) {
            drawChar5x7(ctx, x, y, c, color, clipYMin, clipYMax, alpha);
            return;
        }
        if (y + (7 * scale) < clipYMin || y > clipYMax) return;
        const uint8_t* line = getGlyph(c);

        for (int col = 0; col < 5; ++col) {
            uint8_t bits = line[col];
            for (int bit = 0; bit < 7; ++bit) {
                if (bits & (1 << bit)) {
                    for (int sy = 0; sy < scale; ++sy) {
                        int16_t py = y + (bit * scale) + sy;
                        if (py < clipYMin || py > clipYMax || py < 0 || py >= ctx.height) continue;
                        for (int sx = 0; sx < scale; ++sx) {
                            int16_t px = x + (col * scale) + sx;
                            if (px < 0 || px >= ctx.width) continue;
                            if (alpha >= 255) {
                                ctx.framebuffer[py * ctx.width + px] = color;
                            } else if (alpha > 0) {
                                uint16_t bg = ctx.framebuffer[py * ctx.width + px];
                                ctx.framebuffer[py * ctx.width + px] = blendRGB565(bg, color, alpha);
                            }
                        }
                    }
                }
            }
        }
    }

    // Sobrecarga 1: Estándar sin clipping de viewport o con alpha (8 argumentos)
    static void drawWrappedText(RenderContext& ctx, int16_t startX, int16_t startY, 
                                const char* text, uint16_t color, int16_t maxW, int16_t maxH, 
                                uint8_t alpha = 255) {
        drawWrappedTextClipped(ctx, startX, startY, text, color, maxW, maxH, 0, ctx.height, alpha);
    }

    // Sobrecarga 2: Conveniencia con límites de recorte vertical clipYMin y clipYMax (9 o 10 argumentos)
    static void drawWrappedText(RenderContext& ctx, int16_t startX, int16_t startY, 
                                const char* text, uint16_t color, int16_t maxW, int16_t maxH, 
                                int16_t clipYMin, int16_t clipYMax, uint8_t alpha = 255) {
        drawWrappedTextClipped(ctx, startX, startY, text, color, maxW, maxH, clipYMin, clipYMax, alpha);
    }

    // Implementación canónica de dibujo recortado
    static void drawWrappedTextClipped(RenderContext& ctx, int16_t startX, int16_t startY, 
                                       const char* text, uint16_t color, int16_t maxW, int16_t maxH, 
                                       int16_t clipYMin, int16_t clipYMax,
                                       uint8_t alpha = 255) {
        if (!text || maxW <= 0 || maxH <= 0) return;

        int16_t curX = startX;
        int16_t curY = startY;
        const char* p = text;

        while (*p) {
            if (curY > clipYMax) break;
            if (curY >= startY && (curY - startY) + 7 > maxH) break;

            if (*p == '\n') {
                curY += FONT_LINE_H;
                curX = startX;
                p++;
                continue;
            }

            if (*p == ' ') {
                if ((curX - startX) + FONT_ADVANCE_X > maxW) {
                    curY += FONT_LINE_H;
                    curX = startX;
                } else {
                    curX += FONT_ADVANCE_X;
                }
                p++;
                continue;
            }

            const char* nextWordEnd = p;
            while (*nextWordEnd && *nextWordEnd != ' ' && *nextWordEnd != '\n') {
                nextWordEnd++;
            }
            int16_t wordLen = (int16_t)(nextWordEnd - p);
            int16_t wordPixelWidth = wordLen * FONT_ADVANCE_X;

            if (curX > startX && ((curX - startX) + wordPixelWidth > maxW)) {
                curY += FONT_LINE_H;
                curX = startX;
                if (curY > clipYMax) break;
                if (curY >= startY && (curY - startY) + 7 > maxH) break;
            }

            while (p < nextWordEnd) {
                if ((curX - startX) + FONT_ADVANCE_X > maxW) {
                    curY += FONT_LINE_H;
                    curX = startX;
                    if (curY > clipYMax) break;
                    if (curY >= startY && (curY - startY) + 7 > maxH) break;
                }
                drawChar5x7(ctx, curX, curY, *p, color, clipYMin, clipYMax, alpha);
                curX += FONT_ADVANCE_X;
                p++;
            }
        }
    }

    // Maquetador para diálogos enriquecidos (Advance 7px, Line Height 11px)
    static void drawWrappedTextMsg(RenderContext& ctx, int16_t startX, int16_t startY, 
                                   const char* text, uint16_t color, int16_t maxW, int16_t maxH, 
                                   uint8_t alpha = 255) {
        if (!text || maxW <= 0 || maxH <= 0) return;

        const uint8_t ADV_X = 7;
        const uint8_t LINE_H = 11;

        int16_t curX = startX;
        int16_t curY = startY;
        const char* p = text;

        while (*p) {
            if ((curY - startY) + 8 > maxH) break;

            if (*p == '\n') {
                curY += LINE_H;
                curX = startX;
                p++;
                continue;
            }

            if (*p == ' ') {
                if ((curX - startX) + ADV_X > maxW) {
                    curY += LINE_H;
                    curX = startX;
                } else {
                    curX += ADV_X;
                }
                p++;
                continue;
            }

            const char* nextWordEnd = p;
            while (*nextWordEnd && *nextWordEnd != ' ' && *nextWordEnd != '\n') {
                nextWordEnd++;
            }
            int16_t wordLen = (int16_t)(nextWordEnd - p);
            int16_t wordPixelWidth = wordLen * ADV_X;

            if (curX > startX && ((curX - startX) + wordPixelWidth > maxW)) {
                curY += LINE_H;
                curX = startX;
                if ((curY - startY) + 8 > maxH) break;
            }

            while (p < nextWordEnd) {
                if ((curX - startX) + ADV_X > maxW) {
                    curY += LINE_H;
                    curX = startX;
                    if ((curY - startY) + 8 > maxH) break;
                }
                drawChar6x8(ctx, curX, curY, *p, color, 0, ctx.height, alpha);
                curX += ADV_X;
                p++;
            }
        }
    }

    static int16_t measureTextHeight(const char* text, int16_t maxW) {
        if (!text || maxW <= 0) return 0;
        int16_t curX = 0;
        int16_t lines = 1;
        const char* p = text;

        while (*p) {
            if (*p == '\n') {
                lines++;
                curX = 0;
                p++;
                continue;
            }
            if (*p == ' ') {
                if (curX + FONT_ADVANCE_X > maxW) {
                    lines++;
                    curX = 0;
                } else {
                    curX += FONT_ADVANCE_X;
                }
                p++;
                continue;
            }

            const char* nextWordEnd = p;
            while (*nextWordEnd && *nextWordEnd != ' ' && *nextWordEnd != '\n') {
                nextWordEnd++;
            }
            int16_t wordPixelWidth = (int16_t)(nextWordEnd - p) * FONT_ADVANCE_X;

            if (curX > 0 && (curX + wordPixelWidth > maxW)) {
                lines++;
                curX = 0;
            }

            while (p < nextWordEnd) {
                if (curX + FONT_ADVANCE_X > maxW) {
                    lines++;
                    curX = 0;
                }
                curX += FONT_ADVANCE_X;
                p++;
            }
        }
        return lines * FONT_LINE_H;
    }
};
