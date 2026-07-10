#ifndef TAMAGOTCHI_H
#define TAMAGOTCHI_H

#include "Game.h"
#include <Arduino_GFX_Library.h>
#include <PNGdec.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>

enum Stage { HUEVO, BEBE, CHILD, ADULTO, ANCIANO };
enum Anim { IDLE, HAPPY, SAD, EATING, SLEEPING, SICK, DEAD };

struct PetStats {
    String nombre = "Tama";
    String raza = "tiernito";
    uint8_t hambre = 80;
    uint8_t felicidad = 80;
    uint8_t salud = 100;
    uint8_t energia = 100;
    Stage etapa = HUEVO;
    
    String sombrero = "ninguno";
};

class Tamagotchi : public Game {
public:
    Tamagotchi(Arduino_GFX* gfx, PNG* png, 
               void* (*openFn)(const char*, int32_t*),
               void (*closeFn)(void*),
               int32_t (*readFn)(PNGFILE*, uint8_t*, int32_t),
               int32_t (*seekFn)(PNGFILE*, int32_t),
               int (*drawFn)(PNGDRAW*));

    void init() override;
    void update(uint16_t x, uint16_t y, bool touched) override;
    void redraw() override;
    void saveGame();
    void resetGame(); // Para reiniciar desde Serial

private:
    void drawBackground();
    Arduino_GFX* gfx;
    PNG* png;
    void* (*pngOpen)(const char*, int32_t*);
    void (*pngClose)(void*);
    int32_t (*pngRead)(PNGFILE*, uint8_t*, int32_t);
    int32_t (*pngSeek)(PNGFILE*, int32_t);
    int (*pngDraw)(PNGDRAW*);

    PetStats stats;
    Anim currentAnim = IDLE;
    unsigned long lastTick = 0;
    unsigned long lastAnimUpdate = 0;
    bool needsRedraw = true;

    void loadGame();
    void drawPet();
    void drawUI();
    void renderLayer(String path, int x, int y);
    String getAnimName(Anim a);
    String getStageName(Stage s);
};

#endif
