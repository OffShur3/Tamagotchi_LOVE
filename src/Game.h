// src/Game.h
#pragma once 
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

    // --- MÉTODOS DE CICLO DE VIDA GENÉRICOS (Bypass de Tamagotchi.h) ---
    void saveGame();
    void resetGame();
    void redraw();

private:
    Renderer renderer;
    uint32_t lastFrameTime;
    void handleInput();
};
