// src/tamagotchi/ThoughtBubble.h
#pragma once
#include "../render/RenderObject.h"
#include "../ColorTuner.h"
#include "Pet.h"
#include <math.h>

class ThoughtBubble : public RenderObject {
public:
    ThoughtBubble(Pet& petRef) : pet(petRef) {
        layer = RenderLayer::UI;
        position = {76, 85};
    }

    Size2 getSize() const override { return {22, 18}; }

    void draw(RenderContext& ctx) override {
        if (!visible || !pet.isLightOn() || pet.getState() == PetState::Dead || pet.getStage() == PetStage::Egg) return;

        int iconType = pet.getThoughtIcon();
        if (iconType < 0) return;

        int16_t bobY = position.y + (int16_t)(sinf(millis() / 250.0f) * 2.0f);
        int16_t bx = position.x;

        uint16_t cBorder = colorTunerCorrect565(DOMAIN_THOUGHT, 0x2104);
        uint16_t cBg     = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFFF);

        for (int y = 0; y < 14; ++y) {
            for (int x = 0; x < 20; ++x) {
                if ((x == 0 || x == 19) && (y == 0 || y == 13)) continue;
                uint16_t col = (x == 0 || x == 19 || y == 0 || y == 13) ? cBorder : cBg;
                ctx.drawPixel(bx + x, bobY + y, col);
            }
        }
        ctx.drawPixel(bx + 4, bobY + 14, cBorder);
        ctx.drawPixel(bx + 3, bobY + 15, cBorder);

        drawIcon7x7(ctx, bx + 6, bobY + 3, iconType);
    }

private:
    Pet& pet;

    void drawIcon7x7(RenderContext& ctx, int16_t x, int16_t y, int type) {
        static const uint8_t iconHeart[7]      = {0x00, 0x24, 0x7E, 0x7E, 0x3C, 0x18, 0x00};
        static const uint8_t iconFood[7]       = {0x08, 0x3C, 0x7E, 0x7E, 0x7E, 0x3C, 0x00};
        static const uint8_t iconGame[7]       = {0x00, 0x3C, 0x7E, 0x5A, 0x7E, 0x24, 0x00};
        static const uint8_t iconZzz[7]        = {0x7C, 0x08, 0x10, 0x7C, 0x00, 0x38, 0x38};
        static const uint8_t iconPoop[7]       = {0x18, 0x3C, 0x7E, 0x7E, 0x7E, 0x3C, 0x00};
        static const uint8_t iconCross[7]      = {0x18, 0x18, 0x7E, 0x7E, 0x18, 0x18, 0x00};
        static const uint8_t iconAngry[7]      = {0x42, 0x24, 0x18, 0x7E, 0x18, 0x24, 0x42};
        static const uint8_t iconQuest[7]      = {0x3C, 0x42, 0x04, 0x08, 0x08, 0x00, 0x08};
        static const uint8_t iconWatch[7]      = {0x08, 0x3E, 0x49, 0x4D, 0x41, 0x3E, 0x00};
        static const uint8_t iconAnticipate[7] = {0x08, 0x1C, 0x1C, 0x1C, 0x3E, 0x3E, 0x08};
        static const uint8_t iconCaprice[7]    = {0x38, 0x24, 0x04, 0x18, 0x10, 0x20, 0x3C};

        const uint8_t* data = iconHeart;
        uint16_t rawColor = 0xF800;

        if (type == 1)      { data = iconFood;       rawColor = 0xFA60; }
        else if (type == 2) { data = iconGame;       rawColor = 0x04DF; }
        else if (type == 3) { data = iconZzz;        rawColor = 0x7BEF; }
        else if (type == 4) { data = iconPoop;       rawColor = 0x8200; }
        else if (type == 5) { data = iconCross;      rawColor = 0x07E0; }
        else if (type == 6) { data = iconAngry;      rawColor = 0xF800; }
        else if (type == 7) { data = iconQuest;      rawColor = 0xFEE0; }
        else if (type == 8) { data = iconWatch;      rawColor = 0xFFE0; }
        else if (type == 9) { data = iconAnticipate; rawColor = 0xFDE0; }
        else if (type == 10){ data = iconCaprice;    rawColor = 0xF996; }

        uint16_t col = colorTunerCorrect565(DOMAIN_THOUGHT, rawColor);

        for (int r = 0; r < 7; ++r) {
            uint8_t bits = data[r];
            for (int c = 0; c < 7; ++c) {
                if (bits & (1 << (6 - c))) {
                    ctx.drawPixel(x + c, y + r, col);
                }
            }
        }
    }
};
