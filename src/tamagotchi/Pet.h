#pragma once
#include "PetDef.h"
#include <Arduino.h>
#include <vector>

// =========================================================================
//  CONFIGURACIÓN DEL CICLO DE VIDA DE LA MASCOTA
// =========================================================================
// Modifica esta variable para ajustar la escala de tiempo total:
//   - 300.0f     : Modo Test Express (5 Minutos totales)
//   - 86400.0f   : Modo Test Diario (1 Día = 24 Horas)
//   - 2592000.0f : Modo Realismo (1 Mes = 30 Días)
constexpr float TOTAL_LIFESPAN_SECONDS = 86400.0f; 

// Subdivisión de etapas de vida (Suman 1.0f = 100%)
constexpr float EGG_RATIO    = 0.002f; // 0.2% de la vida (~2.8 minutos en ciclo de 24h)
constexpr float BABY_RATIO   = 0.098f; // 9.8% (~2.3 horas)
constexpr float CHILD_RATIO  = 0.200f; // 20%  (~4.8 horas)
constexpr float ADULT_RATIO  = 0.500f; // 50%  (~12 horas)
constexpr float SENIOR_RATIO = 0.200f; // 20%  (~4.8 horas)
// =========================================================================

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

    void printStats() const;

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
    uint32_t getAge() const { return age; }

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

    uint32_t age = 0;
    uint32_t lastTimestamp = 0;
    
    float actionTimer = 0.0f;
    float poopTimer = 0.0f;
    float autoSaveTimer = 0.0f;

    void checkStateTransitions();
    void checkEvolution();
};
