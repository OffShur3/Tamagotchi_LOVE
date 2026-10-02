// src/tamagotchi/EvolutionFX.h
#pragma once
#include "../render/RenderObject.h"
#include "../render/Blend.h"
#include <Arduino.h>
#include <math.h>

// CAPA 1 (DETRÁS DEL BICHO): Vórtice de rombos que cubre la pantalla completa (172x320)
class EvolutionVortex : public RenderObject {
public:
    EvolutionVortex() {
        layer = RenderLayer::World; // Se dibuja detrás de los personajes
        initParticles();
    }

    Size2 getSize() const override { return {172, 320}; }

    void update(float dt) override {
        for (int i = 0; i < 16; ++i) {
            particles[i].y -= particles[i].speed * dt;
            if (particles[i].y < 30) {
                particles[i].y = 240 + random(0, 20);
                particles[i].x = 20 + random(0, 132);
            }
        }
    }

    void draw(RenderContext& ctx) override {
        if (!visible) return;

        int16_t cx = 86;
        int16_t cy = 150;

        // Cubrir los 320 píxeles de alto completos (Elimina filtraciones del fondo)
        for (int16_t y = 0; y < ctx.height; ++y) {
            for (int16_t x = 0; x < ctx.width; ++x) {
                int d = abs(x - cx) + abs(y - cy);
                uint16_t c = 0x0842; // Azul noche profundo exterior

                if (d < 25)       c = 0x2A09; // Centro radiante
                else if (d < 50)  c = 0x2187;
                else if (d < 80)  c = 0x1925;
                else if (d < 120) c = 0x10A3;

                ctx.drawPixel(x, y, c);
            }
        }

        // Partículas de luz ascendentes (Chispas)
        for (int i = 0; i < 16; ++i) {
            int16_t px = (int16_t)particles[i].x;
            int16_t py = (int16_t)particles[i].y;
            uint16_t pCol = (i % 2 == 0) ? 0xFFFF : 0x87FF;

            ctx.drawPixel(px, py, pCol);
            ctx.drawPixel(px + 1, py, pCol);
            ctx.drawPixel(px, py + 1, pCol);
            ctx.drawPixel(px + 1, py + 1, pCol);
        }
    }

private:
    struct Particle {
        float x, y, speed;
    } particles[16];

    void initParticles() {
        for (int i = 0; i < 16; ++i) {
            particles[i].x = 20 + random(0, 132);
            particles[i].y = 40 + random(0, 200);
            particles[i].speed = 45.0f + random(0, 60);
        }
    }
};

// CAPA 5 (DELANTE DE TODO): Diálogo retro GBA y Fundido Blanco progresivo
class EvolutionOverlay : public RenderObject {
public:
    enum FadeState {
        FADE_NONE,
        FADE_IN_TO_WHITE,     // 0 -> 255
        FADE_OUT_FROM_WHITE,  // 255 -> 0
        FADE_FLASH,           // 255 puro
        FADE_EXIT_TO_WHITE,   // 0 -> 255
        FADE_EXIT_TO_GAME     // 255 -> 0
    };

    EvolutionOverlay() {
        layer = RenderLayer::Popup; // Delante de todo
    }

    Size2 getSize() const override { return {172, 320}; }

    void setStageNames(const String& oldName, const String& newName) {
        _oldName = oldName;
        _newName = newName;
    }

    void setFadeStep(uint8_t step) {
        // Mapeo en 5 pasos exactos de opacidad: 0, 51, 102, 153, 204, 255
        static const uint8_t alphas[6] = {0, 51, 102, 153, 204, 255};
        currentAlpha = (step <= 5) ? alphas[step] : 255;
    }

    void setDialogueActive(bool active, bool isReveal) {
        showDialogue = active;
        revealMode = isReveal;
    }

    void draw(RenderContext& ctx) override {
        if (!visible) return;

        // 1. DIBUJAR CUADRO DE DIÁLOGO GBA EN LA PARTE INFERIOR
        if (showDialogue) {
            drawGbaTextBox(ctx);
        }

        // 2. APLICAR CAPA DE FUNDIDO BLANCO EN 5 PASOS SOBRE TODA LA PANTALLA
        if (currentAlpha > 0) {
            if (currentAlpha >= 255) {
                for (uint32_t i = 0; i < (uint32_t)(ctx.width * ctx.height); ++i) {
                    ctx.framebuffer[i] = 0xFFFF; // Blanco puro
                }
            } else {
                for (uint32_t i = 0; i < (uint32_t)(ctx.width * ctx.height); ++i) {
                    ctx.framebuffer[i] = blendRGB565(ctx.framebuffer[i], 0xFFFF, currentAlpha);
                }
            }
        }
    }

private:
    uint8_t currentAlpha = 0;
    bool showDialogue = false;
    bool revealMode = false;
    String _oldName = "";
    String _newName = "";

    void drawGbaTextBox(RenderContext& ctx) {
        int16_t boxX = 6;
        int16_t boxY = 248;
        int16_t boxW = 160;
        int16_t boxH = 64;

        uint16_t cBorderOut = 0xCE40; // Oro
        uint16_t cBorderIn  = 0xFFFF; // Blanco
        uint16_t cBody      = 0x18C5; // Azul Pizarra

        for (int y = 0; y < boxH; ++y) {
            for (int x = 0; x < boxW; ++x) {
                uint16_t col = cBody;
                if (x < 2 || x >= boxW - 2 || y < 2 || y >= boxH - 2) {
                    col = cBorderOut;
                } else if (x == 3 || x == boxW - 4 || y == 3 || y == boxH - 4) {
                    col = cBorderIn;
                }
                ctx.drawPixel(boxX + x, boxY + y, col);
            }
        }

        if (!revealMode) {
            drawSmallText(ctx, boxX + 12, boxY + 14, "¿Que?", 0xFFFF);
            drawSmallText(ctx, boxX + 12, boxY + 28, "¡Tu mascota esta", 0xFFFF);
            drawSmallText(ctx, boxX + 12, boxY + 42, "evolucionando!", 0xFFFF);
        } else {
            drawSmallText(ctx, boxX + 12, boxY + 14, "¡Felicidades!", 0xFEE0);
            drawSmallText(ctx, boxX + 12, boxY + 28, "¡Ha evolucionado", 0xFFFF);
            drawSmallText(ctx, boxX + 12, boxY + 42, ("a " + _newName + "!").c_str(), 0xFFFF);
        }
    }

    void drawSmallText(RenderContext& ctx, int16_t x, int16_t y, const char* str, uint16_t color) {
        static const uint8_t font5x7[][5] = {
            {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00},
            {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00},
            {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00},
            {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00},
            {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00},
            {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00},
            {0x00, 0x00, 0x00, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08},
            {0x00, 0x60, 0x60, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00},
            {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
            {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
            {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
            {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
            {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
            {0x00, 0x36, 0x36, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00},
            {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00},
            {0x00, 0x00, 0x00, 0x00, 0x00}, {0x02, 0x01, 0x51, 0x09, 0x06},
            {0x00, 0x00, 0x00, 0x00, 0x00}, {0x7E, 0x11, 0x11, 0x11, 0x7E},
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

        int16_t curX = x;
        while (*str) {
            char c = *str++;
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
                            ctx.drawPixel(curX + col, y + bit, color);
                        }
                    }
                }
            }
            curX += 6;
        }
    }
};
