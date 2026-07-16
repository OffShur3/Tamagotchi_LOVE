// src/Game.h
#pragma once // <-- BLINDAJE CONTRA REDEFINICIONES
#include "render/Renderer.h"
#include <Arduino_GFX_Library.h>
#include <stdint.h>

class Game {
public:
    Game(uint16_t width, uint16_t height);
    ~Game() = default;

    void init();
    void tick();

    // Dibuja el frame buffer en el driver de pantalla real
    void flush(Arduino_GFX* display);

private:
    Renderer renderer;
    uint32_t lastFrameTime;
    void handleInput();
};
