// src/tamagotchi/MessagePopup.h
#pragma once
#include "../render/RenderObject.h"
#include "../render/Blend.h"
#include "../ColorTuner.h"
#include <Arduino.h>
#include <math.h>

class MessagePopup : public RenderObject {
public:
    enum PopupState {
        STATE_IDLE,
        STATE_FADE_IN,
        STATE_SUSTAIN,
        STATE_FADE_OUT
    };

    MessagePopup() {
        layer = RenderLayer::Popup;
        visible = false;
        popupState = STATE_IDLE;
        currentStep = 0;
        stepTimer = 0.0f;
        sustainTimer = 0.0f;
        isDreamMode = false;
    }

    Size2 getSize() const override { return {160, 68}; }

    void showMessage(const String& text, float duration = 4.0f, bool isDream = false) {
        messageText = text;
        targetDuration = duration;
        sustainTimer = duration;
        isDreamMode = isDream;
        popupState = STATE_FADE_IN;
        currentStep = 0;
        stepTimer = 0.0f;
        visible = true;
    }

    void dismiss() {
        if (popupState == STATE_FADE_IN || popupState == STATE_SUSTAIN) {
            popupState = STATE_FADE_OUT;
            stepTimer = 0.0f;
        } else if (popupState == STATE_IDLE) {
            visible = false;
        }
    }

    void update(float dt) override {
        if (!visible || popupState == STATE_IDLE) return;

        static const float stepInterval = 0.085f;

        switch (popupState) {
            case STATE_FADE_IN:
                stepTimer += dt;
                if (stepTimer >= stepInterval) {
                    stepTimer -= stepInterval;
                    currentStep++;
                    if (currentStep >= 4) {
                        currentStep = 4;
                        popupState = STATE_SUSTAIN;
                    }
                }
                break;

            case STATE_SUSTAIN:
                sustainTimer -= dt;
                if (sustainTimer <= 0.0f) {
                    popupState = STATE_FADE_OUT;
                    stepTimer = 0.0f;
                }
                break;

            case STATE_FADE_OUT:
                stepTimer += dt;
                if (stepTimer >= stepInterval) {
                    stepTimer -= stepInterval;
                    if (currentStep > 0) {
                        currentStep--;
                    } else {
                        popupState = STATE_IDLE;
                        visible = false;
                    }
                }
                break;

            case STATE_IDLE:
                visible = false;
                break;
        }
    }

    void draw(RenderContext& ctx) override {
        if (!visible || popupState == STATE_IDLE) return;

        static const uint8_t stepAlphas[5] = {51, 102, 153, 204, 255};
        uint8_t currentAlpha = stepAlphas[currentStep];

        int16_t bx = 6;
        int16_t by = 125;
        int16_t bw = 160;
        int16_t bh = 68;

        if (isDreamMode) {
            by += (int16_t)(sinf(millis() / 400.0f) * 1.5f);
        }

        ColorDomain activeDomain = isDreamMode ? DOMAIN_POPUP_DREAM : DOMAIN_POPUP_DAY;

        uint16_t cBorderOut, cBorderIn, cBody, cText;
        if (isDreamMode) {
            cBorderOut = colorTunerCorrect565(activeDomain, 0x18C3);
            cBorderIn  = colorTunerCorrect565(activeDomain, 0x94B2);
            cBody      = colorTunerCorrect565(activeDomain, 0x2965);
            cText      = colorTunerCorrect565(activeDomain, 0xDEFB);
        } else {
            cBorderOut = colorTunerCorrect565(activeDomain, 0x4903);
            cBorderIn  = colorTunerCorrect565(activeDomain, 0xFFFF);
            cBody      = colorTunerCorrect565(activeDomain, 0xF6FA);
            cText      = colorTunerCorrect565(activeDomain, 0x4903);
        }

        for (int y = 0; y < bh; ++y) {
            for (int x = 0; x < bw; ++x) {
                uint16_t col = cBody;
                if (x < 2 || x >= bw - 2 || y < 2 || y >= bh - 2) col = cBorderOut;
                else if (x == 2 || x == bw - 3 || y == 2 || y == bh - 3) col = cBorderIn;
                drawPixelBlended(ctx, bx + x, by + y, col, currentAlpha);
            }
        }

        drawWrappedText(ctx, bx + 8, by + 8, messageText.c_str(), cText, bw - 16, currentAlpha);
    }

private:
    PopupState popupState = STATE_IDLE;
    uint8_t currentStep = 0;
    float stepTimer = 0.0f;
    float sustainTimer = 0.0f;
    float targetDuration = 4.0f;
    bool isDreamMode = false;
    String messageText = "";

    inline void drawPixelBlended(RenderContext& ctx, int16_t x, int16_t y, uint16_t color, uint8_t alpha) {
        if (x >= 0 && x < ctx.width && y >= 0 && y < ctx.height) {
            if (alpha >= 255) {
                ctx.framebuffer[y * ctx.width + x] = color;
            } else if (alpha > 0) {
                uint16_t bg = ctx.framebuffer[y * ctx.width + x];
                ctx.framebuffer[y * ctx.width + x] = blendRGB565(bg, color, alpha);
            }
        }
    }

    void drawWrappedText(RenderContext& ctx, int16_t startX, int16_t startY, const char* str, uint16_t color, int16_t maxW, uint8_t alpha) {
        int16_t curX = startX;
        int16_t curY = startY;
        const char* p = str;
        int16_t maxY = startY + 50;

        while (*p && curY <= maxY) {
            if (*p == '\n') {
                curY += 10;
                curX = startX;
                p++;
                continue;
            }

            if (*p == ' ') {
                if ((curX - startX) + 6 > maxW) {
                    curY += 10;
                    curX = startX;
                } else {
                    curX += 6;
                }
                p++;
                continue;
            }

            const char* nextWordEnd = p;
            while (*nextWordEnd && *nextWordEnd != ' ' && *nextWordEnd != '\n') {
                nextWordEnd++;
            }
            int16_t wordPixelWidth = (int16_t)(nextWordEnd - p) * 6;

            if (curX > startX && ((curX - startX) + wordPixelWidth > maxW)) {
                curY += 10;
                curX = startX;
                if (curY > maxY) break;
            }

            while (p < nextWordEnd && curY <= maxY) {
                if ((curX - startX) + 6 > maxW) {
                    curY += 10;
                    curX = startX;
                    if (curY > maxY) break;
                }
                drawChar5x7(ctx, curX, curY, *p, color, alpha);
                curX += 6;
                p++;
            }
        }
    }

    void drawChar5x7(RenderContext& ctx, int16_t x, int16_t y, char c, uint16_t color, uint8_t alpha) {
        static const uint8_t font5x7[][5] = {
            {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00},
            {0x00, 0x07, 0x00, 0x07, 0x00}, {0x14, 0x7F, 0x14, 0x7F, 0x14},
            {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
            {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00},
            {0x00, 0x1C, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1C, 0x00},
            {0x14, 0x08, 0x3E, 0x08, 0x14}, {0x08, 0x08, 0x3E, 0x08, 0x08},
            {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08},
            {0x00, 0x60, 0x60, 0x00, 0x00}, {0x20, 0x10, 0x08, 0x04, 0x02},
            {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
            {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
            {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
            {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
            {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
            {0x00, 0x36, 0x36, 0x00, 0x00}, {0x00, 0x56, 0x36, 0x00, 0x00},
            {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14},
            {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06},
            {0x32, 0x49, 0x79, 0x41, 0x3E}, {0x7E, 0x11, 0x11, 0x11, 0x7E},
            {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
            {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41},
            {0x7F, 0x09, 0x09, 0x09, 0x01}, {0x3E, 0x41, 0x49, 0x49, 0x7A},
            {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
            {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41},
            {0x7F, 0x40, 0x40, 0x40, 0x40}, {0x7F, 0x02, 0x0C, 0x02, 0x7F},
            {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
            {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E},
            {0x7F, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31},
            {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
            {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x7F, 0x20, 0x18, 0x20, 0x7F},
            {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07},
            {0x61, 0x51, 0x49, 0x45, 0x43}
        };

        char upper = (c >= 'a' && c <= 'z') ? (c - 32) : c;
        int idx = -1;

        if (upper >= ' ' && upper <= 'Z') {
            idx = upper - ' ';
        }

        if (idx >= 0 && idx < 59) {
            for (int col = 0; col < 5; ++col) {
                uint8_t line = font5x7[idx][col];
                for (int bit = 0; bit < 7; ++bit) {
                    if (line & (1 << bit)) {
                        drawPixelBlended(ctx, x + col, y + bit, color, alpha);
                    }
                }
            }
        }
    }
};
