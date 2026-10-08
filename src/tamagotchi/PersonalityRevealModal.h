// src/tamagotchi/PersonalityRevealModal.h
#pragma once
#include "GenericModal.h"
#include "PetDef.h"

class PersonalityRevealModal : public GenericModal {
public:
    PersonalityRevealModal() : GenericModal(10, 30, 152, 240) {
        showCloseButton = false;
        setTransitionTimings(0.15f, 0.0f, 0.15f);
    }

    void showReveal(const PetGenome& gen) {
        genome = gen;
        show("¡NUEVA VIDA!");
    }

    int getClick(uint16_t tx, uint16_t ty) {
        if (!visible || state == STATE_CLOSED) return 0;
        
        int16_t btnY = modalY + modalH - 44;
        int16_t btnX = modalX + 16;
        int16_t btnW = modalW - 32;
        
        if (tx >= btnX && tx <= btnX + btnW && ty >= btnY && ty <= btnY + 34) {
            return 1;
        }
        return 0;
    }

protected:
    PetGenome genome;

    void drawModalContent(RenderContext& ctx) override {
        uint16_t cText = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x4903);
        uint16_t cAccent = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x9000);
        
        int16_t cy = modalY + 26;
        
        TextUtil::drawWrappedText(ctx, modalX + 12, cy, "¡Felicidades! Tu Tama es:", cText, modalW - 24, 22, currentAlpha);
        cy += 14;
        
        TextUtil::drawWrappedText(ctx, modalX + 12, cy, genome.personalityTag.c_str(), cAccent, modalW - 24, 22, currentAlpha);
        cy += 20;

        TextUtil::drawWrappedText(ctx, modalX + 12, cy, "Esto afecta su biologia:", cText, modalW - 24, 22, currentAlpha);
        cy += 14;

        // Interpretación genética al lenguaje humano (Ajustado para no desbordar el cuadro)
        String sHambre = (genome.metabolismRate > 1.05f) ? "Rapida" : ((genome.metabolismRate < 0.95f) ? "Lenta" : "Normal");
        String sSocial = (genome.socialNeed > 1.05f) ? "Exigente" : ((genome.socialNeed < 0.95f) ? "Independ." : "Normal");
        String sSalud = (genome.resilience > 1.05f) ? "Fuerte" : ((genome.resilience < 0.95f) ? "Delicada" : "Normal");
        String sSueno = (genome.sleepPacing > 1.05f) ? "Dormilon" : ((genome.sleepPacing < 0.95f) ? "Inagotable" : "Normal");

        String bullets = "- Hambre: " + sHambre + "\n- Afecto: " + sSocial + "\n- Salud: " + sSalud + "\n- Sueno: " + sSueno;
        TextUtil::drawWrappedText(ctx, modalX + 12, cy, bullets.c_str(), cText, modalW - 24, 50, currentAlpha);
        cy += 46;

        TextUtil::drawWrappedText(ctx, modalX + 12, cy, "¡Cuidalo mucho!", cAccent, modalW - 24, 22, currentAlpha);

        // Renderizar el Botón OK
        int16_t btnY = modalY + modalH - 44;
        int16_t btnX = modalX + 16;
        int16_t btnW = modalW - 32;
        
        uint16_t btnCol = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x54A8);
        uint16_t btnBorder = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x4903);

        for (int16_t y = 0; y < 34; ++y) {
            for (int16_t x = 0; x < btnW; ++x) {
                uint16_t col = (x < 2 || x >= btnW - 2 || y < 2 || y >= 34 - 2) ? btnBorder : btnCol;
                int16_t px = btnX + x;
                int16_t py = btnY + y;
                if (px >= 0 && px < ctx.width && py >= 0 && py < ctx.height) {
                    if (currentAlpha >= 255) {
                        ctx.framebuffer[py * ctx.width + px] = col;
                    } else {
                        uint16_t bg = ctx.framebuffer[py * ctx.width + px];
                        ctx.framebuffer[py * ctx.width + px] = blendRGB565(bg, col, currentAlpha);
                    }
                }
            }
        }
        TextUtil::drawWrappedText(ctx, btnX + 32, btnY + 14, "ENTENDIDO", 0xFFFF, btnW, 34, currentAlpha);
    }
};
