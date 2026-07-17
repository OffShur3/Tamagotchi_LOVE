// src/UI.cpp
#include "UI.h"

// Colores embebidos nativos en RGB565
#define TAMA_UI_BG  0xF6FA // Crema claro (#F5DFBF)
#define TAMA_BROWN  0x4903 // Marrón oscuro (#4C2818)
#define COLOR_GREEN 0x54A8 // Verde Stardew Valley tierra (#569440)
#define MAT_BG      0x1042 // Gris oscuro
#define MAT_OFFLINE 0x4208 // Gris medio/offline

void imprimirCentrado(Arduino_GFX* gfx, const char* texto, int y, int size, uint16_t color) {
    gfx->setTextSize(size);
    gfx->setTextColor(color);
    int anchoPorChar = size * 6;
    int anchoTexto = strlen(texto) * anchoPorChar;
    int x = (172 - anchoTexto) / 2;
    gfx->setCursor(x, y);
    gfx->println(texto);
}

void UIManager::drawStardewPopup(Arduino_GFX* gfx) {
    gfx->fillRoundRect(14, 84, 148, 156, 12, MAT_BG); 
    gfx->fillRoundRect(10, 80, 152, 160, 12, TAMA_BROWN); 
    gfx->fillRoundRect(13, 83, 146, 154, 10, TAMA_UI_BG);

    imprimirCentrado(gfx, "CONEXION", 95, 1, TAMA_BROWN);
    imprimirCentrado(gfx, "FALLIDA", 108, 1, TAMA_BROWN);

    gfx->fillRoundRect(22, 130, 128, 35, 8, TAMA_BROWN); 
    gfx->fillRoundRect(24, 132, 124, 31, 6, COLOR_GREEN); 
    imprimirCentrado(gfx, "Configurar", 142, 1, 0xFFFF);

    gfx->fillRoundRect(22, 180, 128, 35, 8, TAMA_BROWN); 
    gfx->fillRoundRect(24, 182, 124, 31, 6, MAT_OFFLINE); 
    imprimirCentrado(gfx, "Jugar Offline", 192, 1, 0xFFFF);
}

int UIManager::getStardewPopupClick(uint16_t tx, uint16_t ty) {
    if (ty >= 130 && ty <= 165 && tx >= 22 && tx <= 150) {
        return 1;
    }
    if (ty >= 180 && ty <= 215 && tx >= 22 && tx <= 150) {
        return 2;
    }
    return 0;
}

void UIManager::drawUpdatePopup(Arduino_GFX* gfx, const char* title, const char* subtitle) {
    gfx->fillRoundRect(14, 84, 148, 156, 12, MAT_BG); 
    gfx->fillRoundRect(10, 80, 152, 160, 12, TAMA_BROWN); 
    gfx->fillRoundRect(13, 83, 146, 154, 10, TAMA_UI_BG);

    imprimirCentrado(gfx, title, 95, 1, TAMA_BROWN);
    imprimirCentrado(gfx, subtitle, 108, 1, TAMA_BROWN);

    gfx->fillRoundRect(22, 130, 128, 35, 8, TAMA_BROWN); 
    gfx->fillRoundRect(24, 132, 124, 31, 6, COLOR_GREEN); 
    imprimirCentrado(gfx, "Actualizar", 142, 1, 0xFFFF);

    gfx->fillRoundRect(22, 180, 128, 35, 8, TAMA_BROWN); 
    gfx->fillRoundRect(24, 182, 124, 31, 6, MAT_OFFLINE); 
    imprimirCentrado(gfx, "Mas tarde", 192, 1, 0xFFFF);
}

int UIManager::getUpdatePopupClick(uint16_t tx, uint16_t ty) {
    if (ty >= 130 && ty <= 165 && tx >= 22 && tx <= 150) {
        return 1; 
    }
    if (ty >= 180 && ty <= 215 && tx >= 22 && tx <= 150) {
        return 2; 
    }
    return 0;
}

void UIManager::drawUpdateBadge(Arduino_GFX* gfx, int x, int y, int radius) {
    gfx->fillCircle(x, y, radius, 0xF800); 
    gfx->setTextColor(0xFFFF);            
    gfx->setTextSize(1);
    gfx->setCursor(x - 3, y - 4);
    gfx->print("!");
}

bool UIManager::isTouchingBadge(uint16_t touchX, uint16_t touchY, int badgeX, int badgeY, int radius) {
    int dx = (int)touchX - badgeX;
    int dy = (int)touchY - badgeY;
    return (dx * dx + dy * dy) <= (radius * radius); 
}
