// src/tamagotchi/GenericModal.h
#pragma once
#include "../render/RenderObject.h"
#include "../render/TextUtil.h"
#include "../render/Blend.h"
#include "../ColorTuner.h"
#include <Arduino.h>

class GenericModal : public RenderObject {
public:
    enum ModalState {
        STATE_CLOSED,
        STATE_FADE_IN,
        STATE_SUSTAIN,
        STATE_FADE_OUT
    };

    GenericModal(int16_t x = 6, int16_t y = 16, int16_t w = 160, int16_t h = 288) 
        : modalX(x), modalY(y), modalW(w), modalH(h) {
        layer = RenderLayer::Popup;
        visible = false;
        position = {x, y};
    }

    virtual ~GenericModal() = default;

    Size2 getSize() const override { return {modalW, modalH}; }

    void setTransitionTimings(float fadeInSec, float sustainSec, float fadeOutSec) {
        fadeInTime = fadeInSec;
        sustainTime = sustainSec;
        fadeOutTime = fadeOutSec;
    }

    virtual void show(const String& modalTitle = "") {
        title = modalTitle;
        if (fadeInTime <= 0.0f) {
            currentAlpha = 255;
            state = STATE_SUSTAIN;
            stateTimer = sustainTime;
        } else {
            currentAlpha = 0;
            state = STATE_FADE_IN;
            stateTimer = fadeInTime;
        }
        visible = true;
    }

    virtual void dismiss() {
        if (fadeOutTime <= 0.0f) {
            state = STATE_CLOSED;
            visible = false;
            currentAlpha = 0;
        } else if (state == STATE_FADE_IN || state == STATE_SUSTAIN) {
            state = STATE_FADE_OUT;
            stateTimer = fadeOutTime;
        }
    }

    bool isVisible() const { return visible && state != STATE_CLOSED; }
    bool isClosed() const { return state == STATE_CLOSED || !visible; }

    bool isCloseButtonTouched(uint16_t tx, uint16_t ty) const {
        if (!visible) return false;
        int16_t cbX = modalX + modalW - 24;
        int16_t cbY = modalY + 6;
        return (tx >= cbX - 4 && tx <= cbX + 22 && ty >= cbY - 4 && ty <= cbY + 22);
    }

    virtual void update(float dt) override {
        if (!visible || state == STATE_CLOSED) return;

        switch (state) {
            case STATE_FADE_IN:
                if (fadeInTime > 0.0f) {
                    stateTimer -= dt;
                    float progress = 1.0f - constrain(stateTimer / fadeInTime, 0.0f, 1.0f);
                    currentAlpha = (uint8_t)(progress * 255.0f);
                    if (stateTimer <= 0.0f) {
                        currentAlpha = 255;
                        state = STATE_SUSTAIN;
                        stateTimer = sustainTime;
                    }
                } else {
                    currentAlpha = 255;
                    state = STATE_SUSTAIN;
                    stateTimer = sustainTime;
                }
                break;

            case STATE_SUSTAIN:
                if (sustainTime > 0.0f) {
                    stateTimer -= dt;
                    if (stateTimer <= 0.0f) {
                        dismiss();
                    }
                }
                break;

            case STATE_FADE_OUT:
                if (fadeOutTime > 0.0f) {
                    stateTimer -= dt;
                    float progress = constrain(stateTimer / fadeOutTime, 0.0f, 1.0f);
                    currentAlpha = (uint8_t)(progress * 255.0f);
                    if (stateTimer <= 0.0f) {
                        currentAlpha = 0;
                        state = STATE_CLOSED;
                        visible = false;
                    }
                } else {
                    state = STATE_CLOSED;
                    visible = false;
                    currentAlpha = 0;
                }
                break;

            case STATE_CLOSED:
                visible = false;
                break;
        }
    }

    virtual void draw(RenderContext& ctx) override {
        if (!visible || state == STATE_CLOSED) return;

        uint16_t cBorderOut = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x4903); // Marrón Stardew
        uint16_t cBorderIn  = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0xFFFF); // Resalte blanco
        uint16_t cBody      = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0xF6FA); // Crema diurno

        for (int16_t y = 0; y < modalH; ++y) {
            int16_t dy = modalY + y;
            if (dy < 0 || dy >= ctx.height) continue;
            for (int16_t x = 0; x < modalW; ++x) {
                int16_t dx = modalX + x;
                if (dx < 0 || dx >= ctx.width) continue;

                uint16_t col = cBody;
                if (x < 2 || x >= modalW - 2 || y < 2 || y >= modalH - 2) {
                    col = cBorderOut;
                } else if (x == 2 || x == modalW - 3 || y == 2 || y == modalH - 3) {
                    col = cBorderIn;
                }

                if (currentAlpha >= 255) {
                    ctx.framebuffer[dy * ctx.width + dx] = col;
                } else {
                    uint16_t bg = ctx.framebuffer[dy * ctx.width + dx];
                    ctx.framebuffer[dy * ctx.width + dx] = blendRGB565(bg, col, currentAlpha);
                }
            }
        }

        // Título del encabezado
        if (title.length() > 0) {
            uint16_t cTitle = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x4903);
            TextUtil::drawWrappedText(ctx, modalX + 12, modalY + 12, title.c_str(), cTitle, modalW - 40, 14, currentAlpha);
        }

        // Botón [✖] de cierre con opacidad ~65%
        if (showCloseButton) {
            drawCloseButton(ctx);
        }

        drawModalContent(ctx);
    }

protected:
    int16_t modalX, modalY, modalW, modalH;
    ModalState state = STATE_CLOSED;
    float fadeInTime = 0.0f;
    float sustainTime = 0.0f;
    float fadeOutTime = 0.0f;
    float stateTimer = 0.0f;
    uint8_t currentAlpha = 255;
    bool showCloseButton = true;
    String title = "";

    virtual void drawModalContent(RenderContext& ctx) {}

    void drawCloseButton(RenderContext& ctx) {
        int16_t cbX = modalX + modalW - 24;
        int16_t cbY = modalY + 8;
        int16_t cbSize = 16;
        uint8_t effAlpha = (uint8_t)(((uint16_t)currentAlpha * 165) / 255);

        for (int16_t y = 0; y < cbSize; ++y) {
            int16_t dy = cbY + y;
            if (dy < 0 || dy >= ctx.height) continue;
            for (int16_t x = 0; x < cbSize; ++x) {
                int16_t dx = cbX + x;
                if (dx < 0 || dx >= ctx.width) continue;
                uint16_t btnCol = (x == 0 || x == cbSize - 1 || y == 0 || y == cbSize - 1) ? 0x9000 : 0xF800;
                uint16_t bg = ctx.framebuffer[dy * ctx.width + dx];
                ctx.framebuffer[dy * ctx.width + dx] = blendRGB565(bg, btnCol, effAlpha);
            }
        }

        for (int16_t i = 3; i < cbSize - 3; ++i) {
            int16_t dy = cbY + i;
            if (dy < 0 || dy >= ctx.height) continue;
            int16_t dx1 = cbX + i;
            int16_t dx2 = cbX + (cbSize - 1 - i);
            if (dx1 >= 0 && dx1 < ctx.width) {
                uint16_t bg = ctx.framebuffer[dy * ctx.width + dx1];
                ctx.framebuffer[dy * ctx.width + dx1] = blendRGB565(bg, 0xFFFF, currentAlpha);
            }
            if (dx2 >= 0 && dx2 < ctx.width) {
                uint16_t bg = ctx.framebuffer[dy * ctx.width + dx2];
                ctx.framebuffer[dy * ctx.width + dx2] = blendRGB565(bg, 0xFFFF, currentAlpha);
            }
        }
    }
};
