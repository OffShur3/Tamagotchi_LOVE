// src/tamagotchi/MessagePopup.h
#pragma once
#include "../render/RenderObject.h"
#include "../render/TextUtil.h"
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
        targetDuration = 6.0f;
    }

    Size2 getSize() const override { return {160, 74}; }

    void showMessage(const String& text, float duration = 6.0f, bool isDream = false) {
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
        int16_t by = 122;
        int16_t bw = 160;
        int16_t bh = 74;

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

                int16_t px = bx + x;
                int16_t py = by + y;
                if (px >= 0 && px < ctx.width && py >= 0 && py < ctx.height) {
                    if (currentAlpha >= 255) {
                        ctx.framebuffer[py * ctx.width + px] = col;
                    } else if (currentAlpha > 0) {
                        uint16_t bg = ctx.framebuffer[py * ctx.width + px];
                        ctx.framebuffer[py * ctx.width + px] = blendRGB565(bg, col, currentAlpha);
                    }
                }
            }
        }

        // Renderizado del mensaje con fuente enriquecida 6x8 y avance de 7px
        TextUtil::drawWrappedTextMsg(ctx, bx + 8, by + 8, messageText.c_str(), cText, bw - 16, bh - 16, currentAlpha);
    }

private:
    PopupState popupState = STATE_IDLE;
    uint8_t currentStep = 0;
    float stepTimer = 0.0f;
    float sustainTimer = 0.0f;
    float targetDuration = 6.0f;
    bool isDreamMode = false;
    String messageText = "";
};
