#pragma once 
#include "render/Renderer.h"
#include "tamagotchi/Pet.h"
#include "tamagotchi/PetScene.h"
#include <Arduino_GFX_Library.h>
#include <memory>

class Game {
public:
    Game(uint16_t width, uint16_t height);
    ~Game() = default;

    void init();
    void tick();
    void flush(Arduino_GFX* display);

    void saveGame();
    void resetGame();
    void redraw();
    void printStats() const; // <-- EXPONEMOS EL MÉTODO AL KERNEL
    void onTimeSynced();
    
    uint16_t* getFramebuffer() { return renderer.getFramebuffer(); }

private:
    Renderer renderer;
    uint32_t lastFrameTime;

    Pet pet;
    std::shared_ptr<PetScene> petScene = nullptr;

    void handleInput();
};
