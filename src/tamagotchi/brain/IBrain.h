// src/tamagotchi/brain/IBrain.h
#pragma once
#include <Arduino.h>
#include "../PetDef.h"

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

    virtual String getTelemetryJson() const { return "{}"; }
};
