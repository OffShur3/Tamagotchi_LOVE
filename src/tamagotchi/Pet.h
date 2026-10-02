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

    // SOLUCIÓN: La muerte forzada es terminal e irreversible
    void forceState(PetState newState) {
        state = newState;
        if (newState == PetState::Dead) {
            stage = PetStage::Dead;
            health = 0.0f;
            actionTimer = 0.0f; // Sin temporizador: muerte permanente
            Serial.println("[DEBUG] Muerte permanente. El Tamagotchi no resucitará. Usa 'RESET' para una nueva partida.");
        } else {
            actionTimer = 5.0f; // 5 segundos para apreciar animaciones temporales
            Serial.printf("[DEBUG] Estado forzado a: %s (5 segundos)\n", stateToString(newState).c_str());
        }
        save();
    }

    void forcePoop();
    void drainEnergy(float amount = 50.0f);
    void makeSick();
    void accelerateAge(float seconds = 3600.0f);

    void setBrain(std::unique_ptr<IBrain> newBrain);

    void printStats() const;
    // Consulta del icono mental activo para el globo
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
    uint8_t getPoopCount() const { return poopCount; }
    bool isLightOn() const { return lightsOn; }
    uint32_t getAge() const { return (uint32_t)age; }

    String getSpritePath() const;

private:
    String species = "tiernito";
    PetStage stage = PetStage::Egg;
    PetState state = PetState::Idle;

    float hunger = 100.0f;
    float happiness = 100.0f;
    float energy = 100.0f;
    float health = 100.0f;

    uint8_t poopCount = 0;
    bool lightsOn = true;

    float age = 0.0f;
    uint32_t lastTimestamp = 0;
    
    float actionTimer = 0.0f;
    float poopTimer = 0.0f;
    float autoSaveTimer = 0.0f;

    std::unique_ptr<IBrain> brain;

    void checkStateTransitions();
    void checkEvolution();
};
