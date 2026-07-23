#pragma once
#include "PetDef.h"
#include <Arduino.h>
#include <vector>

// =========================================================================
//  CONFIGURACIÓN DEL CICLO DE VIDA DE LA MASCOTA
// =========================================================================
// Modifica esta variable para cambiar la duración entera de la vida:
//   - 300.0f     : Modo Ultra-Rápido (5 minutos totales) para testing de escritorio.
//   - 86400.0f   : Modo Prueba (1 Día = 24 Horas).
//   - 2592000.0f : Modo Real (30 Días = 30 * 24 * 3600 segundos).
constexpr float TOTAL_LIFESPAN_SECONDS = 86400.0f; 

// Subdivisión desigual de etapas (La suma debe dar 1.0f -> 100%)
constexpr float EGG_RATIO    = 0.02f;  // 2%  de la vida
constexpr float BABY_RATIO   = 0.08f;  // 8%  de la vida
constexpr float CHILD_RATIO  = 0.20f;  // 20% de la vida
constexpr float ADULT_RATIO  = 0.50f;  // 50% de la vida
constexpr float SENIOR_RATIO = 0.20f;  // 20% de la vida
// =========================================================================

class Pet {
public:
    Pet();

    void init();
    void update(float dt);

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
    void catchUpTime();
    float autoSaveTimer = 0.0f; // Temporizador para autoguardado de 30s

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
    float actionTimer = 0.0f;
    float poopTimer = 0.0f;
    uint32_t lastTimestamp = 0; // Timestamp Unix del último guardado

    void checkStateTransitions();
    void checkEvolution();
};
