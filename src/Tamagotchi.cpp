#include "Tamagotchi.h"
#include "colors.h"
#include "UI.h"

// Referencias a variables globales en main.cpp
extern int pngOffsetX;
extern int pngOffsetY;
extern int spriteWidth;
extern int spriteHeight;
extern int currentFrame;
extern bool isSpriteSheet; // Corregido: añadido extern

Tamagotchi::Tamagotchi(Arduino_GFX* gfx, PNG* png,
                       void* (*openFn)(const char*, int32_t*),
                       void (*closeFn)(void*),
                       int32_t (*readFn)(PNGFILE*, uint8_t*, int32_t),
                       int32_t (*seekFn)(PNGFILE*, int32_t),
                       int (*drawFn)(PNGDRAW*))
    : gfx(gfx), png(png), pngOpen(openFn), pngClose(closeFn),
      pngRead(readFn), pngSeek(seekFn), pngDraw(drawFn) {}

void Tamagotchi::init() {
    Serial.println("[TAMA] Inicializando...");
    loadGame();
    // No hacemos fillScreen aquí porque redraw() dibujará el fondo
    lastTick = millis();
    lastAnimUpdate = millis();
    needsRedraw = true;
}

void Tamagotchi::resetGame() {
    Serial.println("[TAMA] Reseteando partida...");
    if (SD_MMC.exists("/SAVEGAME.json")) {
        SD_MMC.remove("/SAVEGAME.json");
    }
    stats = PetStats(); 
    init();
}

void Tamagotchi::loadGame() {
    if (!SD_MMC.exists("/SAVEGAME.json")) {
        saveGame();
        return;
    }
    File file = SD_MMC.open("/SAVEGAME.json", "r");
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, file);
    stats.nombre = doc["nombre"] | "Tama";
    stats.raza = doc["raza"] | "tiernito";
    stats.hambre = doc["hambre"] | 80;
    stats.felicidad = doc["felicidad"] | 80;
    stats.etapa = (Stage)(doc["etapa"] | 0);
    stats.sombrero = doc["vestimenta"]["sombrero"] | "ninguno";
    file.close();
}

void Tamagotchi::saveGame() {
    File file = SD_MMC.open("/SAVEGAME.json", "w");
    if (!file) return;
    StaticJsonDocument<1024> doc;
    doc["nombre"] = stats.nombre;
    doc["raza"] = stats.raza;
    doc["hambre"] = stats.hambre;
    doc["etapa"] = (int)stats.etapa;
    JsonObject vest = doc.createNestedObject("vestimenta");
    vest["sombrero"] = stats.sombrero;
    serializeJson(doc, file);
    file.close();
}

void Tamagotchi::renderLayer(String path, int x, int y) {
    if (!SD_MMC.exists(path)) return;
    pngOffsetX = x+2;
    pngOffsetY = y;
    if (png->open(path.c_str(), pngOpen, pngClose, pngRead, pngSeek, pngDraw) == PNG_SUCCESS) {
        png->decode(NULL, 0);
        png->close();
    }
}

void Tamagotchi::drawBackground() {
    isSpriteSheet = false; // El fondo no es un sprite sheet ni se escala
    renderLayer("/tama/ui/bg_main.png", 0, 0);
}

void Tamagotchi::drawPet() {
    spriteWidth = 48; 
    spriteHeight = 48;
    isSpriteSheet = true; // Activar el modo escalado x2 definido en main.cpp

    // Centrar bicho de 96x96 (48x2)
    int px = (gfx->width() - (spriteWidth * 2)) / 2;
    int py = (gfx->height() - (spriteHeight * 2)) / 2;

    String path = "/tama/sprites/base/" + stats.raza + "/" + 
                  getStageName(stats.etapa) + "/" + 
                  getAnimName(currentAnim) + ".png";

    renderLayer(path, px, py);

    if (stats.sombrero != "ninguno") {
        renderLayer("/tama/sprites/vestimenta/sombrero/" + stats.sombrero + ".png", px, py);
    }
}

void Tamagotchi::drawUI() {
    // Dibujamos iconos pequeños sobre el fondo
    renderLayer("/tama/ui/hunger_icon.png", 10, 10);
    gfx->drawRect(30, 15, 50, 8, BGR_WHITE);
    gfx->fillRect(32, 17, (stats.hambre * 46 / 100), 4, COLOR_RED);
}

void Tamagotchi::update(uint16_t x, uint16_t y, bool touched) {
    unsigned long now = millis();

    if (now - lastAnimUpdate > 250) {
        currentFrame = (currentFrame + 1) % 4;
        lastAnimUpdate = now;
        needsRedraw = true;
    }

    if (touched && y > 260) {
        if (x < 86) {
            currentAnim = EATING;
            stats.hambre = (stats.hambre > 90) ? 100 : stats.hambre + 10;
        } else {
            currentAnim = HAPPY;
        }
        esperarSoltar();
        needsRedraw = true;
    }

    if (currentAnim != IDLE) {
        static unsigned long animStart = 0;
        if (animStart == 0) animStart = now;
        if (now - animStart > 3000) {
            currentAnim = IDLE;
            animStart = 0;
            needsRedraw = true;
        }
    }

    if (needsRedraw) {
        redraw();
        needsRedraw = false;
    }
}

void Tamagotchi::redraw() {
    drawBackground(); // Capa 0
    drawPet();        // Capa 1 (Escalada x2)
    drawUI();         // Capa 2
}

String Tamagotchi::getStageName(Stage s) {
    switch(s) {
        case BEBE: return "bebe"; case CHILD: return "child";
        case ADULTO: return "adulto"; case ANCIANO: return "anciano";
        default: return "huevo";
    }
}

String Tamagotchi::getAnimName(Anim a) {
    switch(a) {
        case HAPPY: return "happy"; case SAD: return "sad";
        case EATING: return "eating"; default: return "idle";
    }
}
