// src/tamagotchi/brain/IBrain.h
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "../PetDef.h"

enum ThoughtType {
    THOUGHT_NONE = -1,
    THOUGHT_HEART = 0,         // Amor / Afecto
    THOUGHT_FOOD = 1,          // Apetito / Hambre
    THOUGHT_PLAY = 2,          // Juego / Interacción
    THOUGHT_SLEEP = 3,         // Sueño / Cansancio
    THOUGHT_POOP = 4,          // Suciedad / Caca
    THOUGHT_MED = 5,           // Dolor / Medicina
    THOUGHT_STRESS = 6,        // Estrés agudo
    THOUGHT_CURIOUS = 7,       // Curiosidad exploratoria
    THOUGHT_NOSTALGIA = 8,     // Ensoñación nostálgica (Reloj)
    THOUGHT_DAYDREAM = 8,
    THOUGHT_ANTICIPATION = 9,  // Espera / Anticipación de caricia
    THOUGHT_CAPRICE = 10       // Capricho / Hastío
};

struct PetSensoryInput {
    float hunger;
    float energy;
    float happiness;
    float health;
    int poopCount;
    bool lightsOn;
    bool touched;
    PetStage stage;
    bool isEgg;
    float trust; // Métrica de Apego / Confianza (0.0f a 100.0f)
    bool isBedtimeHours; // Ventana circadiana 22:00 a 04:00 AM
};

class IBrain {
public:
    virtual ~IBrain() = default;

    virtual void init() = 0;
    virtual void applyGenome(const PetGenome& genome) = 0;
    virtual void update(float dt, const PetSensoryInput& input) = 0;
    virtual void emitReward(float amount) = 0;
    virtual PetState getDecision() const = 0;

    virtual void onFed() {}

    virtual float getSpikeRate() const { return 0.0f; }
    virtual float getDopamineLevel() const { return 0.0f; }
    virtual float getStressLevel() const { return 0.0f; }
    virtual float getRuminationLevel() const { return 0.0f; }
    virtual float getHungerMetabolicRate() const { return 1.0f; }
    virtual float getEnergyDrainMultiplier() const { return 1.0f; }
    virtual float getStressMultiplier() const { return 1.0f; }
    virtual float getAffectionWeight() const { return 0.8f; }
    virtual const char* getName() const = 0;
    virtual const char* getArchetypeId() const { return "STD"; }

    virtual int getActiveThought() const { return (int)THOUGHT_NONE; }
    virtual void stimulateNeuron(int idx, float amount) {}
    virtual void nudgeThought(ThoughtType t, const String& reason, float nudge = 0.25f) {}
    virtual void forceThought(ThoughtType t, const String& reason) { nudgeThought(t, reason, 0.35f); }

    virtual void saveState(JsonObject obj) const {}
    virtual void loadState(JsonObjectConst obj) {}

    virtual String getTelemetryJson() const { return "{}"; }
};
