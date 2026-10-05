// src/tamagotchi/NameSelectScene.h
#pragma once
#include "../render/Scene.h"
#include "../render/RenderObject.h"
#include "../render/TextUtil.h"
#include "../render/Blend.h"
#include "../core/PlayerManager.h"
#include <functional>
#include <math.h>

class NameSelectScene : public Scene {
public:
    using OnDoneCallback = std::function<void()>;

    NameSelectScene(OnDoneCallback callback = nullptr, const String& initialName = "") 
        : onDone(callback), presetName(initialName) {}

    void setup(const String& initialName = "") {
        presetName = initialName;
        typedName = initialName.substring(0, 8);
    }

    void enter() override {
        typedName = presetName.substring(0, 8);
        targetIndex = 0.0f;
        currentIndex = 0.0f;
        isDragging = false;
        lastTouchY = 0;
        cursorTimer = 0.0f;
        feedbackTimer = 0.0f;
        activeButton = -1;

        rollerObject = std::make_shared<RollerRenderObject>(*this);
        addObject(rollerObject);
        Serial.printf("[ONBOARDING] Abriendo selector de nombre. Inicial: \"%s\"\n", typedName.c_str());
    }

    void exit() override {
        clearObjects();
        rollerObject = nullptr;
    }

    void update(float dt) override {
        cursorTimer += dt;
        if (cursorTimer >= 0.8f) cursorTimer -= 0.8f;

        if (feedbackTimer > 0.0f) {
            feedbackTimer -= dt;
            if (feedbackTimer <= 0.0f) activeButton = -1;
        }

        // Interpolación y desaceleración del rodillo
        if (!isDragging) {
            float nearest = roundf(targetIndex);
            targetIndex += (nearest - targetIndex) * (1.0f - expf(-18.0f * dt));
        }

        currentIndex += (targetIndex - currentIndex) * (1.0f - expf(-20.0f * dt));

        const float maxSlots = 53.0f;
        while (currentIndex < 0.0f) { currentIndex += maxSlots; targetIndex += maxSlots; }
        while (currentIndex >= maxSlots) { currentIndex -= maxSlots; targetIndex -= maxSlots; }
    }

    void onTouch(uint16_t x, uint16_t y) override {
        // 1. Tambor central Y: [85, 220]
        if (y >= 85 && y <= 220) {
            if (x >= 55 && x <= 118 && y <= 110) {
                targetIndex -= 1.0f;
                return;
            }
            if (x >= 55 && x <= 118 && y >= 195) {
                targetIndex += 1.0f;
                return;
            }

            if (!isDragging) {
                isDragging = true;
                lastTouchY = y;
            } else {
                int16_t dy = y - lastTouchY;
                lastTouchY = y;
                targetIndex -= (float)dy / 20.0f;
            }
            return;
        }

        // 2. Fila 1: Borrar [ ⌫ ] y Aceptar Letra [ ✔ ] Y: [230, 266]
        if (y >= 230 && y <= 266) {
            isDragging = false;
            // Borrar: X: [12, 54]
            if (x >= 12 && x <= 54) {
                activeButton = 0;
                feedbackTimer = 0.15f;
                if (typedName.length() > 0) {
                    typedName.remove(typedName.length() - 1);
                }
                return;
            }
            // Aceptar letra: X: [60, 160]
            if (x >= 60 && x <= 160) {
                activeButton = 1;
                feedbackTimer = 0.15f;
                if (typedName.length() < 8) {
                    char c = getCharFromIndex(roundf(targetIndex));
                    typedName += c;
                }
                return;
            }
        }

        // 3. Confirmar [ LISTO ] Y: [272, 308]
        if (y >= 272 && y <= 308 && x >= 12 && x <= 160) {
            isDragging = false;
            if (typedName.length() >= 2) {
                activeButton = 2;
                feedbackTimer = 0.20f;
                PlayerManager::getInstance().setOwnerName(typedName);
                PlayerManager::getInstance().save();
                Serial.printf("[ONBOARDING] Nombre guardado con exito: %s\n", typedName.c_str());
                if (onDone) onDone();
            }
            return;
        }
    }

    void onTouchReleased() override {
        isDragging = false;
    }

private:
    OnDoneCallback onDone = nullptr;
    String presetName = "";
    String typedName = "";
    float targetIndex = 0.0f;
    float currentIndex = 0.0f;
    bool isDragging = false;
    int16_t lastTouchY = 0;
    float cursorTimer = 0.0f;
    float feedbackTimer = 0.0f;
    int8_t activeButton = -1;

    static const char* getRollerChars() {
        return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ";
    }

    char getCharFromIndex(int idx) const {
        const char* chars = getRollerChars();
        int len = 53;
        int wrapped = ((idx % len) + len) % len;
        return chars[wrapped];
    }

    class RollerRenderObject : public RenderObject {
    public:
        RollerRenderObject(NameSelectScene& parentRef) : parent(parentRef) {
            layer = RenderLayer::World;
            position = {0, 0};
        }

        Size2 getSize() const override { return {172, 320}; }

        void draw(RenderContext& ctx) override {
            for (uint32_t i = 0; i < (uint32_t)(ctx.width * ctx.height); ++i) {
                ctx.framebuffer[i] = 0x1082;
            }

            // Título
            TextUtil::drawWrappedText(ctx, 42, 12, "¿QUIEN SOS?", 0xFEE0, 100, 12, 255);

            // Visor superior de 8 letras
            int16_t boxX = 12;
            int16_t boxY = 28;
            int16_t boxW = 148;
            int16_t boxH = 44;
            drawStardewBox(ctx, boxX, boxY, boxW, boxH, 0xF6FA, 0x4903, 0xFFFF);

            int16_t slotStartX = boxX + 8;
            int16_t slotY = boxY + 11;
            bool blink = (parent.cursorTimer < 0.4f);

            for (int i = 0; i < 8; ++i) {
                int16_t sx = slotStartX + (i * 16);
                for (int gx = 0; gx < 12; ++gx) {
                    ctx.drawPixel(sx + gx, slotY + 20, 0x94B2);
                }

                if (i < parent.typedName.length()) {
                    char c = parent.typedName[i];
                    TextUtil::drawCharScaled(ctx, sx + 1, slotY + 2, c, 0x4903, 2, 0, 320, 255);
                } else if (i == parent.typedName.length() && blink) {
                    for (int dy = 0; dy < 16; ++dy) {
                        ctx.drawPixel(sx + 2, slotY + 2 + dy, 0x54A8);
                        ctx.drawPixel(sx + 3, slotY + 2 + dy, 0x54A8);
                    }
                }
            }

            // Rodillo Cilíndrico Central 3D
            int16_t drumX = 12;
            int16_t drumY = 82;
            int16_t drumW = 148;
            int16_t drumH = 138;
            drawStardewBox(ctx, drumX, drumY, drumW, drumH, 0x18C3, 0x4903, 0x94B2);

            // Ranura horizontal activa (Pill verde Stardew)
            int16_t pillY = 136;
            int16_t pillH = 30;
            for (int y = 0; y < pillH; ++y) {
                for (int x = 4; x < drumW - 4; ++x) {
                    uint16_t col = (y == 0 || y == pillH - 1) ? 0xCE40 : 0x54A8;
                    ctx.drawPixel(drumX + x, pillY + y, col);
                }
            }

            // Flechas guía
            drawArrowUp(ctx, 86, 88, 0xFEE0);
            drawArrowDown(ctx, 86, 212, 0xFEE0);

            // Renderizado trigonométrico 3D
            float curIdx = parent.currentIndex;
            int baseSlot = (int)floorf(curIdx);
            float offsetFrac = curIdx - (float)baseSlot;

            for (int slot = -3; slot <= 3; ++slot) {
                int charIdx = baseSlot + slot;
                char ch = parent.getCharFromIndex(charIdx);

                float diff = (float)slot - offsetFrac;
                float angle = diff * 0.44f;
                if (fabsf(angle) > 1.45f) continue;

                int16_t yCenter = 151 + (int16_t)(sinf(angle) * 52.0f);
                float cosFactor = cosf(angle);
                if (cosFactor < 0.1f) continue;

                uint8_t alpha = (uint8_t)constrain(cosFactor * cosFactor * 255.0f, 40.0f, 255.0f);

                if (fabsf(diff) < 0.40f) {
                    int16_t cx = 86 - (15 / 2);
                    int16_t cy = yCenter - (21 / 2);
                    TextUtil::drawCharScaled(ctx, cx + 1, cy + 1, ch, 0x2104, 3, drumY + 2, drumY + drumH - 2, alpha);
                    TextUtil::drawCharScaled(ctx, cx, cy, ch, 0xFFFF, 3, drumY + 2, drumY + drumH - 2, 255);
                } else if (fabsf(diff) < 1.4f) {
                    int16_t cx = 86 - (10 / 2);
                    int16_t cy = yCenter - (14 / 2);
                    TextUtil::drawCharScaled(ctx, cx, cy, ch, 0xDEFB, 2, drumY + 4, drumY + drumH - 4, alpha);
                } else {
                    int16_t cx = 86 - (5 / 2);
                    int16_t cy = yCenter - (7 / 2);
                    TextUtil::drawChar5x7(ctx, cx, cy, ch, 0x94B2, drumY + 6, drumY + drumH - 6, alpha);
                }
            }

            // Botones inferiores
            bool b0Down = (parent.activeButton == 0);
            drawButton(ctx, 12, 230 + (b0Down ? 2 : 0), 42, 34, 0x9000, 0xF800, "<-", 0xFFFF);

            bool b1Down = (parent.activeButton == 1);
            drawButton(ctx, 60, 230 + (b1Down ? 2 : 0), 100, 34, 0x54A8, 0x4305, "+ LETRA", 0xFFFF);

            bool b2Down = (parent.activeButton == 2);
            bool canFinish = (parent.typedName.length() >= 2);
            uint16_t cFinish = canFinish ? 0xCE40 : 0x4208;
            uint16_t cFinishBorder = canFinish ? 0x9400 : 0x2104;
            const char* finishLabel = canFinish ? "¡CONFIRMAR!" : "MINIMO 2 LETRAS";

            drawButton(ctx, 12, 272 + (b2Down ? 2 : 0), 148, 34, cFinish, cFinishBorder, finishLabel, 0xFFFF);
        }

    private:
        NameSelectScene& parent;

        void drawStardewBox(RenderContext& ctx, int16_t bx, int16_t by, int16_t bw, int16_t bh, uint16_t body, uint16_t bOut, uint16_t bIn) {
            for (int y = 0; y < bh; ++y) {
                for (int x = 0; x < bw; ++x) {
                    uint16_t col = body;
                    if (x < 2 || x >= bw - 2 || y < 2 || y >= bh - 2) col = bOut;
                    else if (x == 2 || x == bw - 3 || y == 2 || y == bh - 3) col = bIn;
                    ctx.drawPixel(bx + x, by + y, col);
                }
            }
        }

        void drawButton(RenderContext& ctx, int16_t bx, int16_t by, int16_t bw, int16_t bh, uint16_t fillCol, uint16_t borderCol, const char* text, uint16_t textCol) {
            for (int y = 0; y < bh; ++y) {
                for (int x = 0; x < bw; ++x) {
                    uint16_t col = (x < 2 || x >= bw - 2 || y < 2 || y >= bh - 2) ? borderCol : fillCol;
                    ctx.drawPixel(bx + x, by + y, col);
                }
            }
            int16_t textLen = strlen(text) * TextUtil::FONT_ADVANCE_X;
            int16_t tx = bx + (bw - textLen) / 2;
            int16_t ty = by + (bh - 7) / 2;
            TextUtil::drawWrappedText(ctx, tx, ty, text, textCol, bw, bh, 255);
        }

        void drawArrowUp(RenderContext& ctx, int16_t cx, int16_t y, uint16_t col) {
            for (int r = 0; r < 5; ++r) {
                for (int c = -r; c <= r; ++c) {
                    ctx.drawPixel(cx + c, y + r, col);
                }
            }
        }

        void drawArrowDown(RenderContext& ctx, int16_t cx, int16_t y, uint16_t col) {
            for (int r = 0; r < 5; ++r) {
                for (int c = -(4 - r); c <= (4 - r); ++c) {
                    ctx.drawPixel(cx + c, y + r, col);
                }
            }
        }
    };

    std::shared_ptr<RollerRenderObject> rollerObject = nullptr;
};
