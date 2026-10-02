// src/Game.h
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
    void printStats() const; 
    void onTimeSynced();
    
    // Métodos delegados al módulo Pet para comandos interactivos y de depuración
    void forcePetState(PetState s) { pet.forceState(s); }
    void startMinigame()           { if (petScene) petScene->startMinigame(); }
    void petFeed()                 { pet.feed(); }
    void petPet()                  { pet.pet(); }
    void petHeal()                 { pet.heal(); }
    void petClean()                { pet.clean(); }
    void petToggleLights()         { pet.toggleLights(); }
    void petForcePoop()            { pet.forcePoop(); }
    void petDrainEnergy()          { pet.drainEnergy(); }
    void petMakeSick()             { pet.makeSick(); }
    void petAccelerateAge()        { pet.accelerateAge(3600.0f); }

    uint16_t* getFramebuffer()     { return renderer.getFramebuffer(); }

private:
    Renderer renderer;
    uint32_t lastFrameTime;

    Pet pet;
    std::shared_ptr<PetScene> petScene = nullptr;

    void handleInput();
};
