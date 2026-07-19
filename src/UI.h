// src/UI.h
#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Helper de impresión centrada universal
void imprimirCentrado(Arduino_GFX* gfx, const char* texto, int y, int size, uint16_t color);

class UIManager {
public:
    // Popup del Portal Cautivo
    static void drawStardewPopup(Arduino_GFX* gfx);
    static int getStardewPopupClick(uint16_t tx, uint16_t ty);
    
    // Popup de Actualización de Software
    static void drawUpdatePopup(Arduino_GFX* gfx, const char* title, const char* subtitle);
    static int getUpdatePopupClick(uint16_t tx, uint16_t ty);

    // Badge rojo de notificación táctil
    static void drawUpdateBadge(Arduino_GFX* gfx, int x, int y, int radius);
    static void drawUpdateBadgeFB(uint16_t* fb, int fbWidth, int fbHeight, int x0, int y0, int radius);
    static bool isTouchingBadge(uint16_t touchX, uint16_t touchY, int badgeX, int badgeY, int radius);
};

#endif
