// src/tamagotchi/ChoiceModal.h
#pragma once
#include "GenericModal.h"

class ChoiceModal : public GenericModal {
public:
    ChoiceModal(int16_t x = 10, int16_t y = 80, int16_t w = 152, int16_t h = 160)
        : GenericModal(x, y, w, h) {
        showCloseButton = false;
        setTransitionTimings(0.12f, 0.0f, 0.12f);
    }

    void setupChoice(const String& modalTitle, const String& modalMessage,
                     const String& primaryLabel, const String& secondaryLabel,
                     uint16_t primaryColor = 0x54A8, uint16_t secondaryColor = 0x4208) {
        title = modalTitle;
        message = modalMessage;
        btn1Text = primaryLabel;
        btn2Text = secondaryLabel;
        btn1Color = primaryColor;
        btn2Color = secondaryColor;
        show(modalTitle);
    }

    int getChoiceClick(uint16_t tx, uint16_t ty) {
        if (!visible || state == STATE_CLOSED) return 0;

        int16_t b1Y = modalY + 54;
        int16_t b2Y = modalY + 104;
        int16_t bX = modalX + 12;
        int16_t bW = modalW - 24;

        if (tx >= bX && tx <= bX + bW) {
            if (ty >= b1Y && ty <= b1Y + 36) return 1;
            if (ty >= b2Y && ty <= b2Y + 36) return 2;
        }
        return 0;
    }

protected:
    String message = "";
    String btn1Text = "Aceptar";
    String btn2Text = "Cancelar";
    uint16_t btn1Color = 0x54A8;
    uint16_t btn2Color = 0x4208;

    void drawModalContent(RenderContext& ctx) override {
        uint16_t cText = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x4903);

        if (message.length() > 0) {
            TextUtil::drawWrappedText(ctx, modalX + 12, modalY + 28, message.c_str(), cText, modalW - 24, 22, currentAlpha);
        }

        // Botón 1 (Primario)
        drawButton(ctx, modalX + 12, modalY + 54, modalW - 24, 34, btn1Color, btn1Text.c_str());

        // Botón 2 (Secundario)
        drawButton(ctx, modalX + 12, modalY + 104, modalW - 24, 34, btn2Color, btn2Text.c_str());
    }

private:
    void drawButton(RenderContext& ctx, int16_t bx, int16_t by, int16_t bw, int16_t bh, uint16_t bodyCol, const char* label) {
        uint16_t cBorder = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x4903);
        uint16_t cFill = colorTunerCorrect565(DOMAIN_POPUP_DAY, bodyCol);

        for (int16_t y = 0; y < bh; ++y) {
            int16_t dy = by + y;
            if (dy < 0 || dy >= ctx.height) continue;
            for (int16_t x = 0; x < bw; ++x) {
                int16_t dx = bx + x;
                if (dx < 0 || dx >= ctx.width) continue;

                uint16_t col = (x < 2 || x >= bw - 2 || y < 2 || y >= bh - 2) ? cBorder : cFill;
                if (currentAlpha >= 255) {
                    ctx.framebuffer[dy * ctx.width + dx] = col;
                } else {
                    uint16_t bg = ctx.framebuffer[dy * ctx.width + dx];
                    ctx.framebuffer[dy * ctx.width + dx] = blendRGB565(bg, col, currentAlpha);
                }
            }
        }

        int16_t textLen = strlen(label) * TextUtil::FONT_ADVANCE_X;
        int16_t tx = bx + (bw - textLen) / 2;
        int16_t ty = by + (bh - 7) / 2;
        TextUtil::drawWrappedText(ctx, tx, ty, label, 0xFFFF, bw, bh, currentAlpha);
    }
};
