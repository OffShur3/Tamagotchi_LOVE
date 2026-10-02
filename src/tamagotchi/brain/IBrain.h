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
};

class IBrain {
public:
    virtual ~IBrain() = default;

    virtual void init() = 0;
    virtual void update(float dt, const PetSensoryInput& input) = 0;
    virtual void emitReward(float amount) = 0;
    virtual PetState getDecision() const = 0;

    // Notificación de que el usuario alimentó a la mascota
    virtual void onFed() {}

    virtual float getSpikeRate() const { return 0.0f; }
    virtual float getDopamineLevel() const { return 0.0f; }
    virtual float getStressLevel() const { return 0.0f; }
    virtual const char* getName() const = 0;
};
