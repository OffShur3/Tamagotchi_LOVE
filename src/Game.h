// src/Game.h
#pragma once 
#include "render/Renderer.h"
#include "tamagotchi/Pet.h"
#include "tamagotchi/PetScene.h"
#include "tamagotchi/NameSelectScene.h"
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
    
    bool isPetDead() const { 
        return (pet.getStage() == PetStage::Dead || pet.getState() == PetState::Dead); 
    }

    bool hasDeathTouchOccurred() { return petScene ? petScene->hasDeathTouch() : false; }
    void clearDeathTouch() { if (petScene) petScene->clearDeathTouch(); }

    void forcePetState(PetState s) { pet.forceState(s); }
    void startMinigame()           { if (petScene) petScene->startMinigame(); }
    void startAnimationTest()      { if (petScene) petScene->startAnimationTest(); }
    
    void petFeed()                 { pet.feed(); }
    void petPet()                  { pet.pet(); }
    void petHeal()                 { pet.heal(); }
    void petClean()                { pet.clean(); }
    void petToggleLights()         { pet.toggleLights(); }
    void petForcePoop()            { pet.forcePoop(); }
    void petMaxPoop()              { pet.maxPoop(); }
    void petDrainEnergy()          { pet.drainEnergy(); }
    void petMakeSick()             { pet.makeSick(); }
    void petStarve()               { pet.starve(); }
    void petMakeBored()            { pet.makeBored(); }
    void petMakeHappy()            { pet.makeHappy(); }
    void petGodMode()              { pet.godMode(); }
    void petRevive()               { pet.revive(); }
    void petEvolveNext()           { pet.evolveNextStage(); }
    void petJumpStage(PetStage s)  { pet.jumpToStage(s); }
    void petAccelerateAge()        { pet.accelerateAge(3600.0f); }

    void stimulateBrain(int neuron, float val) {
        if (pet.getBrain()) static_cast<Brain*>(pet.getBrain())->stimulateNeuron(neuron, val);
    }
    void nudgeThought(ThoughtType t, const String& reason, float nudge = 0.25f) {
        if (pet.getBrain()) static_cast<Brain*>(pet.getBrain())->nudgeThought(t, reason, nudge);
    }

    uint16_t* getFramebuffer() { return renderer.getFramebuffer(); }

private:
    Renderer renderer;
    uint32_t lastFrameTime;

    Pet pet;
    std::shared_ptr<PetScene> petScene = nullptr;
    std::shared_ptr<NameSelectScene> nameSelectScene = nullptr;

    void handleInput();
};
