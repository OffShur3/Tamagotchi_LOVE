#pragma once
#include "../render/RenderObject.h"
#include <time.h>

class ClockWidget : public RenderObject {
public:
    // Flag global para ocultar el reloj si no hay sincronización de Internet
    inline static bool isSyncedWithInternet = false; 

    ClockWidget() {
        layer = RenderLayer::UI;
        position = {110, 8}; // Esquina superior derecha
    }

    Size2 getSize() const override { return {54, 18}; }

    void draw(RenderContext& ctx) override {
        // SOLO DIBUJAR EN PANTALLA SI SE SINCRO POR INTERNET
        if (!visible || !isSyncedWithInternet) return;

        // Paleta Stardew Valley
        uint16_t borderDark  = 0x2104; // Marrón oscuro
        uint16_t bgBox       = 0x4903; // Marrón Madera (#4C2818)
        uint16_t textColor   = 0xFFEC; // Crema / Dorado (#FFF3A0)

        int16_t bx = position.x;
        int16_t by = position.y;
        int16_t bw = 54;
        int16_t bh = 18;

        for (int16_t y = 0; y < bh; ++y) {
            for (int16_t x = 0; x < bw; ++x) {
                uint16_t col = bgBox;
                if (x == 0 || y == 0 || x == bw - 1 || y == bh - 1) {
                    col = borderDark;
                }
                ctx.drawPixel(bx + x, by + y, col);
            }
        }

        struct tm timeinfo;
        char timeStr[6] = "08:00";
        if (getLocalTime(&timeinfo, 10)) {
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        }

        drawText5x7(ctx, bx + 12, by + 5, timeStr, textColor);
    }

private:
    void drawText5x7(RenderContext& ctx, int16_t x, int16_t y, const char* str, uint16_t color) {
        static const uint8_t digits[11][5] = {
            {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
            {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
            {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
            {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
            {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
            {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
            {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
            {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
            {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
            {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
            {0x00, 0x36, 0x36, 0x00, 0x00}  // :
        };

        int16_t cx = x;
        while (*str) {
            char c = *str++;
            int idx = -1;
            if (c >= '0' && c <= '9') idx = c - '0';
            else if (c == ':') idx = 10;

            if (idx >= 0) {
                for (int col = 0; col < 5; ++col) {
                    uint8_t line = digits[idx][col];
                    for (int bit = 0; bit < 7; ++bit) {
                        if (line & (1 << bit)) {
                            ctx.drawPixel(cx + col, y + bit, color);
                        }
                    }
                }
            }
            cx += 6;
        }
    }
};
