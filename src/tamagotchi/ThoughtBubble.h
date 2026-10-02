// src/tamagotchi/ThoughtBubble.h
#pragma once
#include "../render/RenderObject.h"
#include "Pet.h"
#include <math.h>

class ThoughtBubble : public RenderObject {
public:
    ThoughtBubble(Pet& petRef) : pet(petRef) {
        layer = RenderLayer::UI;
        position = {76, 85}; // Flotando sobre la cabeza de la mascota
    }

    Size2 getSize() const override { return {22, 18}; }

    void draw(RenderContext& ctx) override {
        // Ocultar si duerme con la luz apagada o si está muerto
        if (!visible || !pet.isLightOn() || pet.getState() == PetState::Dead) return;

        int iconType = pet.getThoughtIcon();
        if (iconType < 0) return; // Mente tranquila, sin pensamientos urgentes

        // Efecto visual de levitación sinusoidal suave
        int16_t bobY = position.y + (int16_t)(sinf(millis() / 250.0f) * 2.0f);
        int16_t bx = position.x;

        // 1. Contenedor de nube blanca con borde marrón retro (20x15 px)
        uint16_t cBorder = 0x2104; 
        uint16_t cBg     = 0xFFFF; 

        for (int y = 0; y < 14; ++y) {
            for (int x = 0; x < 20; ++x) {
                if ((x == 0 || x == 19) && (y == 0 || y == 13)) continue;
                uint16_t col = (x == 0 || x == 19 || y == 0 || y == 13) ? cBorder : cBg;
                ctx.drawPixel(bx + x, bobY + y, col);
            }
        }
        // Rabillo hacia la cabeza
        ctx.drawPixel(bx + 4, bobY + 14, cBorder);
        ctx.drawPixel(bx + 3, bobY + 15, cBorder);

        // 2. Dibujar el icono interior de 7x7 correspondiente
        drawIcon7x7(ctx, bx + 6, bobY + 3, iconType);
    }

private:
    Pet& pet;

    void drawIcon7x7(RenderContext& ctx, int16_t x, int16_t y, int type) {
        // Bitmaps 7x7: 0=Corazón, 1=Comida, 2=Juego, 3=Sueño, 4=Caca, 5=Medicina, 6=Estrés/Enojo, 7=Curiosidad
        static const uint8_t iconHeart[7] = {0x00, 0x24, 0x7E, 0x7E, 0x3C, 0x18, 0x00};
        static const uint8_t iconFood[7]  = {0x08, 0x3C, 0x7E, 0x7E, 0x7E, 0x3C, 0x00};
        static const uint8_t iconGame[7]  = {0x00, 0x3C, 0x7E, 0x5A, 0x7E, 0x24, 0x00};
        static const uint8_t iconZzz[7]   = {0x7C, 0x08, 0x10, 0x7C, 0x00, 0x38, 0x38};
        static const uint8_t iconPoop[7]  = {0x18, 0x3C, 0x7E, 0x7E, 0x7E, 0x3C, 0x00};
        static const uint8_t iconCross[7] = {0x18, 0x18, 0x7E, 0x7E, 0x18, 0x18, 0x00};
        static const uint8_t iconAngry[7] = {0x42, 0x24, 0x18, 0x7E, 0x18, 0x24, 0x42};
        static const uint8_t iconQuest[7] = {0x3C, 0x42, 0x04, 0x08, 0x08, 0x00, 0x08};

        const uint8_t* data = iconHeart;
        uint16_t col = 0xF800; // Rojo

        if (type == 1)      { data = iconFood;  col = 0xFA60; } // Naranja (Comida)
        else if (type == 2) { data = iconGame;  col = 0x04DF; } // Azul (Juego)
        else if (type == 3) { data = iconZzz;   col = 0x7BEF; } // Gris azulado (Sueño)
        else if (type == 4) { data = iconPoop;  col = 0x8200; } // Marrón (Caca)
        else if (type == 5) { data = iconCross; col = 0x07E0; } // Verde (Medicina)
        else if (type == 6) { data = iconAngry; col = 0xF800; } // Rojo (Enojo/Estrés)
        else if (type == 7) { data = iconQuest; col = 0xFEE0; } // Amarillo (Curiosidad)

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
