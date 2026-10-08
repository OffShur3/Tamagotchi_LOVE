// src/tamagotchi/Pet.h
#pragma once
#include "PetDef.h"
#include "brain/IBrain.h"
#include "brain/Brain.h"
#include <Arduino.h>
#include <memory>
#include <vector>

constexpr float TOTAL_LIFESPAN_SECONDS = 86400.0f; 

constexpr float EGG_RATIO    = 0.002f; 
constexpr float BABY_RATIO   = 0.098f; 
constexpr float CHILD_RATIO  = 0.200f; 
constexpr float ADULT_RATIO  = 0.500f; 
constexpr float SENIOR_RATIO = 0.200f; 

class Pet {
public:
    Pet();

    void init();
    void update(float dt);
    void catchUpTime();

    void feed();
    void pet();
    void heal();
    void clean();
    void toggleLights();

    void forceState(PetState newState) {
        state = newState;
        if (newState == PetState::Dead) {
            health = 0.0f;
            actionTimer = 0.0f; 
            Serial.printf("[PET-ALERT] Mascota fallecida. Etapa: %s | Mostrando %s/dead.png\n", 
                          stageToString(stage).c_str(), stageToString(stage).c_str());
        } else {
            actionTimer = 5.0f; 
            Serial.printf("[DEBUG] Estado forzado a: %s (5 segundos)\n", stateToString(newState).c_str());
        }
        save();
    }

    // Suite de pruebas
    void forcePoop();
    void maxPoop() { poopCount = 3; poopTimer = 0.0f; save(); }
    void drainEnergy(float amount = 50.0f);
    void makeSick();
    void starve() { hunger = 0.0f; state = PetState::Sad; save(); }
    void makeBored() { happiness = 10.0f; state = PetState::Sad; save(); }
    void makeHappy() { happiness = 100.0f; if (brain) brain->emitReward(1.0f); save(); }
    void godMode() {
        hunger = 100.0f; happiness = 100.0f; energy = 100.0f; health = 100.0f;
        poopCount = 0; trust = 100.0f; state = PetState::Idle; save();
        Serial.println("[TEST] Modo Dios activado: 100% en todas las estadisticas.");
    }
    void revive() {
        state = PetState::Idle;
        health = 100.0f; energy = 100.0f; hunger = 100.0f; happiness = 100.0f;
        poopCount = 0; trust = 50.0f;
        if (stage == PetStage::Dead) stage = PetStage::Baby;
        Serial.println("[TEST] Mascota revivida con exito!");
        save();
    }
    void evolveNextStage();
    void jumpToStage(PetStage s);
    void accelerateAge(float seconds = 3600.0f);

    void setBrain(std::unique_ptr<IBrain> newBrain);
    IBrain* getBrain() const { return brain.get(); }

    void printStats() const;
    int getThoughtIcon() const {
        return brain ? static_cast<Brain*>(brain.get())->getActiveThought() : -1;
    }

    bool load();
    bool save();
    void reset();

    String getSpecies() const { return species; }
    std::vector<String> discoverInstalledSpecies();

    PetStage getStage() const { return stage; }
    PetState getState() const { return state; }
    float getHunger() const { return hunger; }
    float getHappiness() const { return happiness; }
    float getEnergy() const { return energy; }
    float getHealth() const { return health; }
    float getTrust() const { return trust; }
    uint8_t getPoopCount() const { return poopCount; }
    bool isLightOn() const { return lightsOn; }
    uint32_t getAge() const { return (uint32_t)age; }
    uint32_t getDnaSeed() const { return dnaSeed; }

    String getSpritePath() const;

private:
    String species = "tiernito";
    PetStage stage = PetStage::Egg;
    PetState state = PetState::Idle;
    uint32_t dnaSeed = 0;

    float hunger = 100.0f;
    float happiness = 100.0f;
    float energy = 100.0f;
    float health = 100.0f;
    float trust = 5.0f; // Apego / Confianza (0.0f a 100.0f)

    uint8_t poopCount = 0;
    bool lightsOn = true;

    float age = 0.0f;
    uint32_t lastTimestamp = 0;
    
    float actionTimer = 0.0f;
    float poopTimer = 0.0f;
    float autoSaveTimer = 0.0f;

    // Motor Digestivo Biológico
    float digestiveTransitTimer = 0.0f;
    bool digesting = false;
    float poopExposureTimer = 0.0f;

    std::unique_ptr<IBrain> brain;

    void checkStateTransitions();
    void checkEvolution();
};
