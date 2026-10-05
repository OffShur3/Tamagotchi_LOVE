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
        position = {68, 78};
    }

    Size2 getSize() const override { return {34, 26}; }

    void draw(RenderContext& ctx) override {
        if (!visible || !pet.isLightOn() || pet.getState() == PetState::Dead || pet.getStage() == PetStage::Egg) return;

        int iconType = pet.getThoughtIcon();
        if (iconType < 0) return;

        int16_t bobY = position.y + (int16_t)(sinf(millis() / 250.0f) * 2.0f);
        int16_t bx = position.x;

        uint16_t cBorder = colorTunerCorrect565(DOMAIN_THOUGHT, 0x2104);
        uint16_t cBg     = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFFF);

        for (int y = 0; y < 22; ++y) {
            for (int x = 0; x < 32; ++x) {
                if ((x <= 1 && y <= 1) || (x >= 30 && y <= 1) ||
                    (x <= 1 && y >= 20) || (x >= 30 && y >= 20)) {
                    continue;
                }

                bool isBorder = (x == 0 || x == 31 || y == 0 || y == 21 ||
                                (x <= 2 && y <= 2) || (x >= 29 && y <= 2) ||
                                (x <= 2 && y >= 19) || (x >= 29 && y >= 19));

                uint16_t col = isBorder ? cBorder : cBg;
                ctx.drawPixel(bx + x, bobY + y, col);
            }
        }

        ctx.drawPixel(bx + 8,  bobY + 22, cBorder);
        ctx.drawPixel(bx + 9,  bobY + 22, cBorder);
        ctx.drawPixel(bx + 7,  bobY + 23, cBorder);
        ctx.drawPixel(bx + 8,  bobY + 23, cBg);
        ctx.drawPixel(bx + 6,  bobY + 24, cBorder);
        ctx.drawPixel(bx + 5,  bobY + 25, cBorder);

        drawIcon11x11(ctx, bx + 10, bobY + 5, iconType);
    }

    static void drawIcon11x11Clipped(RenderContext& ctx, int16_t x, int16_t y, int type, int16_t clipYMin, int16_t clipYMax) {
        if (y + 11 < clipYMin || y > clipYMax || type < 0 || type > 10) return;

        uint16_t pal[5] = {0, 0, 0, 0, 0};

        static const uint8_t rawIcons[11][121] = {
            // 0: HEART
            {
                0,0,1,1,0,0,0,1,1,0,0,
                0,1,3,3,1,0,1,1,1,1,0,
                1,3,3,1,1,1,1,1,1,1,1,
                1,1,1,1,1,1,1,1,1,1,1,
                1,1,1,1,1,1,1,1,1,1,1,
                0,1,1,1,1,1,1,1,1,1,0,
                0,0,1,1,1,1,1,1,1,0,0,
                0,0,0,1,1,1,1,1,0,0,0,
                0,0,0,0,1,1,1,0,0,0,0,
                0,0,0,0,0,1,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0
            },
            // 1: PANCHO
            {
                0,0,0,0,0,0,0,0,3,3,0,
                0,0,0,1,1,1,1,3,3,3,0,
                0,0,1,2,2,4,4,1,3,0,0,
                0,1,2,2,4,4,2,2,1,0,0,
                3,1,2,4,4,2,2,2,1,0,0,
                3,3,1,4,4,2,2,2,1,3,0,
                0,3,1,2,2,4,4,2,1,3,3,
                0,0,1,2,2,2,4,4,2,1,3,
                0,0,0,1,2,2,2,2,1,0,0,
                0,0,3,3,1,1,1,1,0,0,0,
                0,3,3,0,0,0,0,0,0,0,0
            },
            // 2: GAME
            {
                0,1,1,1,1,1,1,1,1,1,0,
                1,1,1,1,1,1,1,1,1,1,1,
                1,0,2,0,1,1,1,3,1,4,1,
                1,2,2,2,1,1,3,1,4,1,1,
                1,0,2,0,1,1,1,1,1,1,1,
                1,1,1,1,1,1,1,1,1,1,1,
                1,1,1,1,1,1,1,1,1,1,1,
                1,1,0,0,1,1,1,0,0,1,1,
                1,0,0,0,0,1,0,0,0,0,1,
                0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0
            },
            // 3: SLEEP
            {
                0,0,1,1,1,0,0,3,3,3,0,
                0,1,1,0,0,0,0,0,0,3,0,
                1,1,0,0,0,0,0,0,3,0,0,
                1,1,0,0,0,0,0,3,3,3,0,
                1,1,0,0,0,0,0,0,0,0,0,
                1,1,0,0,0,0,4,4,0,0,0,
                0,1,1,0,0,0,0,4,0,0,0,
                0,0,1,1,1,0,4,4,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0
            },
            // 4: POOP
            {
                0,0,0,4,0,0,4,0,0,0,0,
                0,0,4,0,0,4,0,0,0,0,0,
                0,0,0,0,1,1,0,0,0,0,0,
                0,0,0,1,2,2,1,0,0,0,0,
                0,0,1,1,2,2,1,1,0,0,0,
                0,1,2,2,1,1,2,2,1,0,0,
                1,2,2,2,2,2,2,2,2,1,0,
                1,1,3,3,3,3,3,3,1,1,0,
                1,2,2,2,2,2,2,2,2,2,1,
                0,1,1,1,1,1,1,1,1,1,0,
                0,0,0,0,0,0,0,0,0,0,0
            },
            // 5: MED
            {
                0,0,0,0,0,0,1,1,1,0,0,
                0,0,0,0,0,1,3,1,1,1,0,
                0,0,0,0,1,3,1,1,1,1,1,
                0,0,0,1,1,1,1,1,1,1,1,
                0,0,1,1,1,1,2,2,1,1,0,
                0,1,1,1,1,2,2,2,2,0,0,
                0,1,1,2,2,2,2,2,0,0,0,
                1,2,2,2,2,2,0,0,0,0,0,
                1,2,2,2,0,0,0,0,0,0,0,
                0,1,1,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0
            },
            // 6: STRESS
            {
                0,1,1,0,0,0,0,0,1,1,0,
                1,2,2,1,0,0,0,1,2,2,1,
                1,2,2,1,0,0,0,1,2,2,1,
                0,1,1,1,1,1,1,1,1,1,0,
                0,0,0,1,3,3,3,1,0,0,0,
                0,0,0,1,3,3,3,1,0,0,0,
                0,0,0,1,3,3,3,1,0,0,0,
                0,1,1,1,1,1,1,1,1,1,0,
                1,2,2,1,0,0,0,1,2,2,1,
                1,2,2,1,0,0,0,1,2,2,1,
                0,1,1,0,0,0,0,0,1,1,0
            },
            // 7: CURIOUS
            {
                0,0,1,1,1,1,1,0,0,0,0,
                0,1,3,3,1,1,1,1,0,0,0,
                1,3,1,0,0,0,1,1,1,0,0,
                1,1,0,0,0,0,1,1,1,0,0,
                0,0,0,0,0,1,1,1,0,0,0,
                0,0,0,0,1,1,1,0,0,0,0,
                0,0,0,1,1,1,0,0,0,0,0,
                0,0,0,1,1,1,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,1,1,1,0,0,0,0,0,
                0,0,0,1,1,1,0,0,0,0,0
            },
            // 8: NOSTALGIA
            {
                0,0,0,0,2,2,2,0,0,0,0,
                0,0,0,0,1,2,1,0,0,0,0,
                0,0,1,1,1,1,1,1,1,0,0,
                0,1,1,3,3,3,3,3,1,1,0,
                1,1,3,3,3,4,3,3,3,1,1,
                1,1,3,3,3,4,3,3,3,1,1,
                1,1,3,3,3,4,4,4,3,1,1,
                0,1,1,3,3,3,3,3,1,1,0,
                0,0,1,1,1,1,1,1,1,0,0,
                0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0
            },
            // 9: ANTICIPATION
            {
                0,0,0,0,0,2,0,0,0,0,0,
                0,0,0,0,1,1,1,0,0,0,0,
                0,0,0,1,1,4,1,1,0,0,0,
                0,0,0,1,4,4,1,1,0,0,0,
                0,0,1,1,4,1,1,1,1,0,0,
                0,0,1,1,1,1,1,1,1,0,0,
                0,1,1,1,1,1,1,1,1,1,0,
                1,1,1,1,1,1,1,1,1,1,1,
                0,2,2,2,2,2,2,2,2,2,0,
                0,0,0,0,3,3,0,0,0,0,0,
                0,0,0,0,3,3,0,0,0,0,0
            },
            // 10: CAPRICE
            {
                0,0,0,1,1,1,1,1,0,0,0,
                0,0,1,1,2,2,2,1,1,0,0,
                0,1,1,0,0,0,0,2,1,1,0,
                1,1,0,1,1,1,0,0,1,1,0,
                1,2,0,1,3,1,1,0,2,1,0,
                1,2,0,1,1,1,1,0,2,1,0,
                1,2,0,0,0,0,0,0,2,1,0,
                0,1,1,2,2,2,2,2,1,1,0,
                0,0,1,1,1,1,1,1,1,0,0,
                0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0
            }
        };

        switch (type) {
            case 0:
                pal[1] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xF800);
                pal[2] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xA800);
                pal[3] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFFF);
                break;
            case 1:
                pal[1] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xCE40);
                pal[2] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFA60);
                pal[3] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xF800);
                pal[4] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFE0);
                break;
            case 2:
                pal[1] = colorTunerCorrect565(DOMAIN_THOUGHT, 0x31A6);
                pal[2] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xCE79);
                pal[3] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xF800);
                pal[4] = colorTunerCorrect565(DOMAIN_THOUGHT, 0x07FF);
                break;
            case 3:
                pal[1] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFE0);
                pal[2] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xCE40);
                pal[3] = colorTunerCorrect565(DOMAIN_THOUGHT, 0x07FF);
                pal[4] = colorTunerCorrect565(DOMAIN_THOUGHT, 0x94B2);
                break;
            case 4:
                pal[1] = colorTunerCorrect565(DOMAIN_THOUGHT, 0x8200);
                pal[2] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xCE40);
                pal[3] = colorTunerCorrect565(DOMAIN_THOUGHT, 0x4100);
                pal[4] = colorTunerCorrect565(DOMAIN_THOUGHT, 0x94B2);
                break;
            case 5:
                pal[1] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xF800);
                pal[2] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFFF);
                pal[3] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFEE0);
                break;
            case 6:
                pal[1] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xF800);
                pal[2] = colorTunerCorrect565(DOMAIN_THOUGHT, 0x7800);
                pal[3] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFFF);
                break;
            case 7:
                pal[1] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFE0);
                pal[2] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFA60);
                pal[3] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFFF);
                break;
            case 8:
                pal[1] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xCE40);
                pal[2] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFE0);
                pal[3] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFFF);
                pal[4] = colorTunerCorrect565(DOMAIN_THOUGHT, 0x0000);
                break;
            case 9:
                pal[1] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFDE0);
                pal[2] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xCE40);
                pal[3] = colorTunerCorrect565(DOMAIN_THOUGHT, 0x2104);
                pal[4] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFFF);
                break;
            case 10:
                pal[1] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xF996);
                pal[2] = colorTunerCorrect565(DOMAIN_THOUGHT, 0x89D7);
                pal[3] = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFFF);
                break;
        }

        const uint8_t* ptr = rawIcons[type];
        for (int r = 0; r < 11; ++r) {
            int16_t py = y + r;
            if (py >= clipYMin && py <= clipYMax && py >= 0 && py < ctx.height) {
                for (int c = 0; c < 11; ++c) {
                    int16_t px = x + c;
                    uint8_t idx = ptr[r * 11 + c];
                    if (idx > 0 && idx < 5 && px >= 0 && px < ctx.width) {
                        ctx.framebuffer[py * ctx.width + px] = pal[idx];
                    }
                }
            }
        }
    }

    static void drawIcon11x11(RenderContext& ctx, int16_t x, int16_t y, int type) {
        drawIcon11x11Clipped(ctx, x, y, type, 0, ctx.height);
    }

private:
    Pet& pet;
};
