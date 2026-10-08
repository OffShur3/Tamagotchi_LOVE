// src/tamagotchi/brain/SimpleReactiveBrain.h
#pragma once
#include "IBrain.h"

class SimpleReactiveBrain : public IBrain {
public:
    void init() override { decision = PetState::Idle; }
    void applyGenome(const PetGenome& genome) override {} // Implementación vacía para el bot simple

    void update(float dt, const PetSensoryInput& in) override {
        if (!in.lightsOn) {
            decision = (in.energy >= 100.0f) ? PetState::Idle : PetState::Sleeping;
            return;
        }
        if (in.health < 30.0f || in.poopCount >= 2) {
            decision = PetState::Sick;
        } else if (in.hunger < 25.0f || in.happiness < 25.0f) {
            decision = PetState::Sad;
        } else {
            decision = PetState::Idle;
        }
    }

    void emitReward(float amount) override {}
    PetState getDecision() const override { return decision; }
    const char* getName() const override { return "FSM Reactivo (1996)"; }

    String getTelemetryJson() const override {
        return "{\"name\":\"FSM Reactivo 1996\",\"dopamine\":0,\"spikeRate\":0,\"thought\":\"-1\"}";
    }

private:
    PetState decision = PetState::Idle;
};
