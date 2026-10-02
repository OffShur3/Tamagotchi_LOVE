// src/tamagotchi/brain/Brain.h
#pragma once
#include "IBrain.h"
#include "Neuron.h"
#include "Synapse.h"
#include <vector>

enum ThoughtType {
    THOUGHT_NONE = -1,
    THOUGHT_HEART = 0,    
    THOUGHT_FOOD = 1,     
    THOUGHT_PLAY = 2,     
    THOUGHT_SLEEP = 3,    
    THOUGHT_POOP = 4,     
    THOUGHT_MED = 5,      
    THOUGHT_STRESS = 6,   
    THOUGHT_CURIOUS = 7   
};

class Brain : public IBrain {
public:
    Brain();
    ~Brain() override = default;

    void init() override;
    void update(float dt, const PetSensoryInput& input) override;
    void emitReward(float amount) override;
    void onFed() override; // Saciador de la neurona de hambre

    PetState getDecision() const override;
    float getStressLevel() const override;
    float getDopamineLevel() const override { return dopamine; }
    float getSpikeRate() const override { return spikeRate; }
    const char* getName() const override { return "SNN Explainable Homeostatic (v2)"; }

    int getActiveThought() const { return (int)currentThought; }

    float getHungerDrive() const { return driveHunger; }
    float getSleepDrive() const { return driveSleep; }
    float getSocialDrive() const { return driveSocial; }
    float getDistressDrive() const { return driveDistress; }

private:
    std::vector<Neuron> neurons;
    std::vector<Synapse> synapses;

    float dopamine = 0.5f;       
    float spikeRate = 0.0f;      
    int globalSpikeCount = 0;
    float spikeTimer = 0.0f;

    float driveHunger = 0.0f;
    float driveSleep = 0.0f;
    float driveSocial = 0.0f;
    float driveDistress = 0.0f;

    ThoughtType currentThought = THOUGHT_NONE;
    ThoughtType lastLoggedThought = THOUGHT_NONE;
    PetState currentDecision = PetState::Idle;
    float actionDurationTimer = 0.0f;
    float memoryRecallTimer = 0.0f;
    float nextRecallInterval = 30.0f;

    static const int S_HUNGER   = 0;
    static const int S_FATIGUE  = 1;
    static const int S_BOREDOM  = 2;
    static const int S_PAIN     = 3;
    static const int S_TOUCH    = 4;

    static const int M_SLEEP    = 5;
    static const int M_PLAY     = 6;
    static const int M_SICK     = 7;
    static const int M_IDLE     = 8;

    void setupConnectome();
    void processPlasticity(float dt);
    void evaluateThoughts(const PetSensoryInput& input);
};
