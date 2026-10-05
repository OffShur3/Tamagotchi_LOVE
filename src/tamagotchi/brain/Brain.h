// src/tamagotchi/brain/Brain.h
#pragma once
#include "IBrain.h"
#include "Neuron.h"
#include "Synapse.h"
#include <vector>

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

class Brain : public IBrain {
public:
    Brain();
    ~Brain() override = default;

    void init() override;
    void update(float dt, const PetSensoryInput& input) override;
    void emitReward(float amount) override;
    void onFed() override;

    PetState getDecision() const override;
    float getStressLevel() const override;
    float getDopamineLevel() const override { return dopamine; }
    float getSpikeRate() const override { return spikeRate; }
    float getRuminationLevel() const override { return ruminationLevel; }
    float getHungerMetabolicRate() const override { return hungerMetabolicMult; }
    float getEnergyDrainMultiplier() const override { return energyDrainMult; }
    float getStressMultiplier() const override { return stressMult; }
    float getAffectionWeight() const override {
        for (const auto& syn : synapses) {
            if (syn.preIndex == S_TOUCH && syn.postIndex == M_LOVE) {
                return syn.weight;
            }
        }
        return 0.9f;
    }

    const char* getName() const override { return "SNN Cognitive-Attachment Decoupled (v3.3)"; }

    int getActiveThought() const { return (int)currentThought; }
    String getActiveThoughtName() const;
    String getLastReason() const { return lastThoughtReason; }

    float getHungerDrive() const { return driveHunger; }
    float getSleepDrive() const { return driveSleep; }
    float getSocialDrive() const { return driveSocial; }
    float getDistressDrive() const { return driveDistress; }

    void stimulateNeuron(int idx, float amount);
    void nudgeThought(ThoughtType t, const String& reason, float nudge = 0.25f);
    void forceThought(ThoughtType t, const String& reason) {
        nudgeThought(t, reason, 0.35f);
    }

    String getTelemetryJson() const override;

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
    float lastTrust = 5.0f;

    ThoughtType currentThought = THOUGHT_NONE;
    String lastThoughtReason = "Mente en reposo";
    float forcedThoughtTimer = 0.0f;
    
    float ruminationLevel = 0.0f;
    float timeSinceLastTouch = 0.0f;
    bool isEggStage = false;

    float hungerMetabolicMult = 1.0f;
    float energyDrainMult = 1.0f;
    float stressMult = 1.0f;

    PetState currentDecision = PetState::Idle;
    float actionDurationTimer = 0.0f;
    float memoryRecallTimer = 0.0f;
    float nextRecallInterval = 30.0f;

    // Conectoma de 10 Neuronas: 5 Sensoriales + 5 Motoras/Emocionales
    static const int S_HUNGER   = 0;
    static const int S_FATIGUE  = 1;
    static const int S_BOREDOM  = 2;
    static const int S_PAIN     = 3;
    static const int S_TOUCH    = 4;

    static const int M_SLEEP    = 5;
    static const int M_PLAY     = 6;
    static const int M_SICK     = 7;
    static const int M_IDLE     = 8;
    static const int M_LOVE     = 9; // 10ª Neurona: Apego Seguro / Serenidad

    void setupConnectome();
    void processPlasticity(float dt, float trust, PetStage stage);
    void evaluateThoughts(const PetSensoryInput& input);
    void computePsychosomaticMultipliers();
    void overflowThoughtToMotor(ThoughtType t);
};
