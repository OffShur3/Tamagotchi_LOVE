// src/tamagotchi/ConfigHelpModal.h
#pragma once
#include "GenericModal.h"
#include "ThoughtBubble.h"
#include "../core/PlayerManager.h"
#include <WiFi.h>
#include <functional>

class ConfigHelpModal : public GenericModal {
public:
    using EditNameCallback = std::function<void()>;

    ConfigHelpModal() : GenericModal(6, 16, 160, 288) {
        currentTab = 0;
        scrollY = 0;
        maxScrollY = 180;
        isDragging = false;
        dragStartY = 0;
        dragStartScrollY = 0;
        setTransitionTimings(0.0f, 0.0f, 0.0f);
    }

    void setOnEditNameRequested(EditNameCallback cb) {
        onEditNameRequested = cb;
    }

    bool isVisible() const { return visible && state != STATE_CLOSED; }

    void onTouch(uint16_t tx, uint16_t ty) {
        if (!visible || state == STATE_CLOSED) return;

        // 1. Botón [✖] de cierre
        if (isCloseButtonTouched(tx, ty)) {
            dismiss();
            return;
        }

        // 2. Barra de navegación por pestañas: Y: [42, 66]
        if (ty >= modalY + 26 && ty <= modalY + 50) {
            isDragging = false;
            if (tx >= modalX + 4 && tx <= modalX + 52) {
                if (currentTab != 0) { currentTab = 0; scrollY = 0; recalculateMaxScroll(); }
            } else if (tx >= modalX + 54 && tx <= modalX + 102) {
                if (currentTab != 1) { currentTab = 1; scrollY = 0; recalculateMaxScroll(); }
            } else if (tx >= modalX + 104 && tx <= modalX + 154) {
                if (currentTab != 2) { currentTab = 2; scrollY = 0; recalculateMaxScroll(); }
            }
            return;
        }

        // 3. Toque en el botón [ ✎ CAMBIAR NOMBRE ] en la pestaña SIS
        if (currentTab == 2) {
            int16_t btnY = modalY + 52 - scrollY + 44;
            if (ty >= btnY && ty <= btnY + 24 && tx >= modalX + 10 && tx <= modalX + 146) {
                dismiss();
                if (onEditNameRequested) {
                    onEditNameRequested();
                }
                return;
            }
        }

        // 4. Área de scroll Y: [68, 290]
        if (ty >= modalY + 52 && ty <= modalY + modalH - 8) {
            if (!isDragging) {
                isDragging = true;
                dragStartY = ty;
                dragStartScrollY = scrollY;
            } else {
                int16_t dy = dragStartY - ty;
                scrollY = constrain(dragStartScrollY + dy, 0, maxScrollY);
            }
        }
    }

    void onTouchReleased() {
        isDragging = false;
    }

protected:
    uint8_t currentTab = 0;
    int16_t scrollY = 0;
    int16_t maxScrollY = 180;
    bool isDragging = false;
    int16_t dragStartY = 0;
    int16_t dragStartScrollY = 0;
    EditNameCallback onEditNameRequested = nullptr;

    void recalculateMaxScroll() {
        int16_t viewH = modalH - 60;
        int16_t totalH = computeTabContentHeight();
        maxScrollY = max(0, totalH - viewH + 12);
        scrollY = constrain(scrollY, 0, maxScrollY);
    }

    int16_t computeTabContentHeight() {
        if (currentTab == 0) {
            return 320;
        } else if (currentTab == 1) {
            return 210;
        } else {
            return 190;
        }
    }

    void drawModalContent(RenderContext& ctx) override {
        int16_t bx = modalX;
        int16_t by = modalY;
        int16_t bw = modalW;
        int16_t bh = modalH;

        uint16_t cBorderOut = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x4903);
        uint16_t cBorderIn  = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0xFFFF);
        uint16_t cGold      = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0xCE40);

        // Barra de pestañas
        const char* tabNames[3] = {"MENTE", "BIO", "SIS"};
        int16_t tabX[3] = {(int16_t)(bx + 8), (int16_t)(bx + 57), (int16_t)(bx + 106)};
        int16_t tabW = 46;
        int16_t tabH = 18;
        int16_t tabY = by + 28;

        for (int i = 0; i < 3; ++i) {
            bool isActive = (i == currentTab);
            uint16_t tBg  = isActive ? cGold : 0xCE59;
            uint16_t tTxt = isActive ? 0xFFFF : cBorderOut;

            for (int ty = 0; ty < tabH; ++ty) {
                for (int tx = 0; tx < tabW; ++tx) {
                    uint16_t col = (tx == 0 || tx == tabW - 1 || ty == 0 || ty == tabH - 1) ? cBorderOut : tBg;
                    ctx.drawPixel(tabX[i] + tx, tabY + ty, col);
                }
            }
            int16_t offsetTextX = (int16_t)((tabW - (strlen(tabNames[i]) * TextUtil::FONT_ADVANCE_X)) / 2);
            TextUtil::drawWrappedText(ctx, tabX[i] + offsetTextX, tabY + 5, tabNames[i], tTxt, tabW, tabH, currentAlpha);
        }

        for (int x = 4; x < bw - 4; ++x) {
            ctx.drawPixel(bx + x, by + 48, cBorderOut);
            ctx.drawPixel(bx + x, by + 49, cBorderIn);
        }

        int16_t clipTop = by + 52;
        int16_t clipBottom = by + bh - 8;
        int16_t viewH = clipBottom - clipTop;

        renderTabContent(ctx, bx + 8, clipTop, clipTop, clipBottom, viewH);

        if (maxScrollY > 0) {
            int16_t trackH = viewH;
            int16_t thumbH = max(18, trackH - (maxScrollY / 2));
            int16_t thumbY = clipTop + (int16_t)(((float)scrollY / maxScrollY) * (trackH - thumbH));
            int16_t barX = bx + bw - 6;

            for (int y = 0; y < trackH; ++y) {
                ctx.drawPixel(barX, clipTop + y, 0xD69A);
            }
            for (int y = 0; y < thumbH; ++y) {
                ctx.drawPixel(barX - 1, thumbY + y, cBorderOut);
                ctx.drawPixel(barX,     thumbY + y, cGold);
            }
        }
    }

private:
    void renderTabContent(RenderContext& ctx, int16_t startX, int16_t startY, int16_t clipTop, int16_t clipBottom, int16_t viewH) {
        uint16_t cText    = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x4903);
        uint16_t cSubText = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x6185);
        uint16_t cAccent  = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0x9000);
        uint16_t cGold    = colorTunerCorrect565(DOMAIN_POPUP_DAY, 0xCE40);

        int16_t curY = startY - scrollY;

        if (currentTab == 0) {
            struct ThoughtEntry {
                int iconId;
                const char* title;
                const char* desc;
            };

            static const ThoughtEntry entries[10] = {
                {1,  "PANCHO",     "Hambre bajo 40% o apetito por rumiacion neuronal."},
                {0,  "AMOR",       "Gratitud y afecto sincero por caricias y dopamina."},
                {2,  "JUEGO",      "Aburrimiento o soledad, requiere estimulo social."},
                {3,  "SUENO",      "Energia baja en penumbra, apaga la luz del cuarto."},
                {4,  "CACA",       "Incomodidad por suciedad, limpia el suelo tocando."},
                {5,  "BOTIQUIN",   "Salud deteriorada grave, administra medicina ya."},
                {6,  "ESTRES",     "Dolor agudo o abandono, sufrimiento en aumento."},
                {8,  "RELOJ",      "Nostalgia en paz feliz, todas las barras al 100%."},
                {9,  "ESPERA",     "Anticipacion paciente tras mucho tiempo sin mimos."},
                {10, "CAPRICHO",   "Hastio leve exploratorio buscando novedad activa."}
            };

            for (int i = 0; i < 10; ++i) {
                int16_t descH = TextUtil::measureTextHeight(entries[i].desc, 136);
                int16_t itemH = 13 + descH + 8;

                if (curY + itemH >= clipTop && curY <= clipBottom) {
                    ThoughtBubble::drawIcon11x11Clipped(ctx, startX + 2, curY + 1, entries[i].iconId, clipTop, clipBottom);
                    TextUtil::drawWrappedText(ctx, startX + 18, curY, entries[i].title, cAccent, 126, 12, clipTop, clipBottom);
                    TextUtil::drawWrappedText(ctx, startX + 4, curY + 13, entries[i].desc, cSubText, 136, descH + 2, clipTop, clipBottom);
                }
                curY += itemH;
            }
        } else if (currentTab == 1) {
            TextUtil::drawWrappedText(ctx, startX, curY, "REGLAS BIOLOGICAS", cAccent, 144, 12, clipTop, clipBottom);
            curY += 14;

            TextUtil::drawWrappedText(ctx, startX, curY, "1. HAMBRE Y TRANSITO:", cText, 144, 12, clipTop, clipBottom);
            curY += 11;
            int16_t h1 = TextUtil::measureTextHeight("El estomago digiere en 15-25 min. Comer demasiado acelera el transito un 30%. En ayunas no hace caca.", 144);
            TextUtil::drawWrappedText(ctx, startX, curY, "El estomago digiere en 15-25 min. Comer demasiado acelera el transito un 30%. En ayunas no hace caca.", cSubText, 144, h1 + 2, clipTop, clipBottom);
            curY += h1 + 8;

            TextUtil::drawWrappedText(ctx, startX, curY, "2. APEGO Y CONFIANZA:", cText, 144, 12, clipTop, clipBottom);
            curY += 11;
            int16_t h2 = TextUtil::measureTextHeight("Alimentar a tiempo y limpiar cacas genera confianza. El abandono prolongado drena el apego.", 144);
            TextUtil::drawWrappedText(ctx, startX, curY, "Alimentar a tiempo y limpiar cacas genera confianza. El abandono prolongado drena el apego.", cSubText, 144, h2 + 2, clipTop, clipBottom);
            curY += h2 + 8;

            TextUtil::drawWrappedText(ctx, startX, curY, "3. HIGIENE Y DOLOR:", cText, 144, 12, clipTop, clipBottom);
            curY += 11;
            int16_t h3 = TextUtil::measureTextHeight("Cacas sin limpiar por mas de 15 min causan dolor fisico e inducen enfermedad.", 144);
            TextUtil::drawWrappedText(ctx, startX, curY, "Cacas sin limpiar por mas de 15 min causan dolor fisico e inducen enfermedad.", cSubText, 144, h3 + 2, clipTop, clipBottom);
            curY += h3 + 8;

            TextUtil::drawWrappedText(ctx, startX, curY, "4. DESCANSO NOCTURNO:", cText, 144, 12, clipTop, clipBottom);
            curY += 11;
            int16_t h4 = TextUtil::measureTextHeight("Duerme solo a oscuras. Despertarlo de golpe con menos de 75% penaliza el apego con -10%.", 144);
            TextUtil::drawWrappedText(ctx, startX, curY, "Duerme solo a oscuras. Despertarlo de golpe con menos de 75% penaliza el apego con -10%.", cSubText, 144, h4 + 2, clipTop, clipBottom);
            curY += h4 + 8;
        } else {
            TextUtil::drawWrappedText(ctx, startX, curY, "TAMA OS KERNEL", cAccent, 144, 12, clipTop, clipBottom);
            curY += 12;
            TextUtil::drawWrappedText(ctx, startX, curY, "Version: v0.0.46\nRed: SNN 10 Neuronas", cText, 144, 22, clipTop, clipBottom);
            curY += 24;

            String owner = PlayerManager::getInstance().getOwnerName();
            TextUtil::drawWrappedText(ctx, startX, curY, "IDENTIDAD:", cAccent, 144, 12, clipTop, clipBottom);
            curY += 11;
            TextUtil::drawWrappedText(ctx, startX, curY, ("DUENO: " + owner).c_str(), cText, 144, 12, clipTop, clipBottom);
            curY += 15;

            int16_t btnBX = startX + 2;
            int16_t btnBY = curY;
            int16_t btnBW = 136;
            int16_t btnBH = 20;

            if (btnBY + btnBH >= clipTop && btnBY <= clipBottom) {
                for (int y = 0; y < btnBH; ++y) {
                    int16_t py = btnBY + y;
                    if (py < clipTop || py > clipBottom) continue;
                    for (int x = 0; x < btnBW; ++x) {
                        int16_t px = btnBX + x;
                        uint16_t col = (x < 2 || x >= btnBW - 2 || y < 2 || y >= btnBH - 2) ? cText : cGold;
                        ctx.drawPixel(px, py, col);
                    }
                }
                TextUtil::drawWrappedText(ctx, btnBX + 14, btnBY + 6, "[ CAMBIAR NOMBRE ]", 0xFFFF, btnBW - 10, btnBH, clipTop, clipBottom);
            }
            curY += btnBH + 12;

            bool isOnline = (WiFi.status() == WL_CONNECTED);
            TextUtil::drawWrappedText(ctx, startX, curY, "CONECTIVIDAD:", cText, 144, 12, clipTop, clipBottom);
            curY += 11;
            TextUtil::drawWrappedText(ctx, startX, curY, isOnline ? "WiFi: Conectado (NTP listo)" : "WiFi: Modo Local (Offline)", isOnline ? 0x0500 : cSubText, 144, 12, clipTop, clipBottom);
            curY += 18;

            TextUtil::drawWrappedText(ctx, startX, curY, "COMANDOS SERIAL:", cAccent, 144, 12, clipTop, clipBottom);
            curY += 12;
            int16_t hCmd = TextUtil::measureTextHeight("- HELP: Manual completo\n- TUNECOLOURS: Calibrador\n- STATS: Diagnostico SNN\n- RESET_PLAYER: Borrar dueno", 144);
            TextUtil::drawWrappedText(ctx, startX, curY, "- HELP: Manual completo\n- TUNECOLOURS: Calibrador\n- STATS: Diagnostico SNN\n- RESET_PLAYER: Borrar dueno", cSubText, 144, hCmd + 2, clipTop, clipBottom);
            curY += hCmd + 8;
        }
    }
};
