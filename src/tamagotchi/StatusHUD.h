// src/tamagotchi/StatusHUD.h
#pragma once
#include "../render/RenderObject.h"
#include "../ColorTuner.h"
#include "Pet.h"

class StatusHUD : public RenderObject {
public:
    StatusHUD(Pet& petRef) : pet(petRef) {
        layer = RenderLayer::UI;
        position = {6, 6};
    }

    Size2 getSize() const override { return {100, 28}; }

    void draw(RenderContext& ctx) override {
        if (!visible) return;

        // Barra 1: Hambre (Icono Corazón ♥ + Barra Naranja)
        drawIcon5x5(ctx, position.x, position.y + 1, colorTunerCorrect565(DOMAIN_HUD, 0xF800), 0);
        drawLargeBar(ctx, position.x + 8, position.y, pet.getHunger(), colorTunerCorrect565(DOMAIN_HUD, 0xFA60));

        // Barra 2: Felicidad (Icono Estrella ★ + Barra Dorada)
        drawIcon5x5(ctx, position.x, position.y + 9, colorTunerCorrect565(DOMAIN_HUD, 0xFEE0), 1);
        drawLargeBar(ctx, position.x + 8, position.y + 8, pet.getHappiness(), colorTunerCorrect565(DOMAIN_HUD, 0xFDE0));

        // Barra 3: Energía (Icono Rayo ⚡ + Barra Cian)
        drawIcon5x5(ctx, position.x, position.y + 17, colorTunerCorrect565(DOMAIN_HUD, 0x07FF), 2);
        drawLargeBar(ctx, position.x + 8, position.y + 16, pet.getEnergy(), colorTunerCorrect565(DOMAIN_HUD, 0x05BF));

        // Alerta de Salud crítica o Cacas acumuladas
        if (pet.getHealth() < 40.0f || pet.getPoopCount() > 0) {
            if ((millis() / 500) % 2 == 0) {
                drawWarningIcon(ctx, position.x + 84, position.y + 8);
            }
        }
    }

private:
    Pet& pet;

    void drawLargeBar(RenderContext& ctx, int16_t x, int16_t y, float value, uint16_t barColor) {
        int16_t barW = 68;
        int16_t barH = 7;
        uint16_t cBorder = colorTunerCorrect565(DOMAIN_HUD, 0x2104);
        uint16_t cEmpty  = colorTunerCorrect565(DOMAIN_HUD, 0x10A2);

        for (int16_t bx = 0; bx < barW; ++bx) {
            ctx.drawPixel(x + bx, y, cBorder);
            ctx.drawPixel(x + bx, y + barH - 1, cBorder);
        }
        for (int16_t by = 0; by < barH; ++by) {
            ctx.drawPixel(x, y + by, cBorder);
            ctx.drawPixel(x + barW - 1, y + by, cBorder);
        }

        int16_t fillW = (int16_t)((constrain(value, 0.0f, 100.0f) * (barW - 2)) / 100.0f);
        for (int16_t by = 1; by < barH - 1; ++by) {
            for (int16_t bx = 1; bx < barW - 1; ++bx) {
                uint16_t col = (bx <= fillW) ? barColor : cEmpty;
                ctx.drawPixel(x + bx, y + by, col);
            }
        }
    }

    void drawIcon5x5(RenderContext& ctx, int16_t x, int16_t y, uint16_t color, int type) {
        static const uint8_t iconHeart[5] = {0x0A, 0x1F, 0x1F, 0x0E, 0x04};
        static const uint8_t iconStar[5]  = {0x04, 0x15, 0x0E, 0x15, 0x04};
        static const uint8_t iconBolt[5]  = {0x06, 0x0C, 0x1F, 0x06, 0x0C};

        const uint8_t* iconData = (type == 0) ? iconHeart : (type == 1 ? iconStar : iconBolt);

        for (int row = 0; row < 5; ++row) {
            uint8_t bits = iconData[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (1 << (4 - col))) {
                    ctx.drawPixel(x + col, y + row, color);
                }
            }
        }
    }

    void drawWarningIcon(RenderContext& ctx, int16_t x, int16_t y) {
        uint16_t c = colorTunerCorrect565(DOMAIN_HUD, 0xF800);
        for (int i = 0; i < 9; ++i) {
            ctx.drawPixel(x + 4, y + i, c);
            ctx.drawPixel(x + i, y + 4, c);
        }
    }
};
