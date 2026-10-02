// src/tamagotchi/brain/Brain.cpp
#include "Brain.h"
#include <math.h>

Brain::Brain() {
    neurons.resize(9);
    setupConnectome();
}

void Brain::init() {
    currentDecision = PetState::Idle;
    currentThought = THOUGHT_NONE;
    lastLoggedThought = THOUGHT_NONE;
    dopamine = 0.5f;
    spikeRate = 0.0f;
    globalSpikeCount = 0;
    spikeTimer = 0.0f;
    actionDurationTimer = 0.0f;
    memoryRecallTimer = 0.0f;
    nextRecallInterval = 30.0f;
}

void Brain::setupConnectome() {
    neurons[S_HUNGER].threshold = 1.0f;  neurons[S_HUNGER].leak = 0.5f;
    neurons[S_FATIGUE].threshold = 1.0f; neurons[S_FATIGUE].leak = 0.5f;
    neurons[S_BOREDOM].threshold = 1.0f; neurons[S_BOREDOM].leak = 0.5f;
    neurons[S_PAIN].threshold = 1.0f;    neurons[S_PAIN].leak = 0.5f;
    neurons[S_TOUCH].threshold = 0.5f;   neurons[S_TOUCH].leak = 0.8f; 

    neurons[M_SLEEP].threshold = 2.0f;   neurons[M_SLEEP].leak = 0.4f; 
    neurons[M_PLAY].threshold = 1.8f;    neurons[M_PLAY].leak = 0.6f; 
    neurons[M_SICK].threshold = 1.5f;    neurons[M_SICK].leak = 0.3f;
    neurons[M_IDLE].threshold = 1.0f;    neurons[M_IDLE].leak = 0.5f;

    synapses.clear();
    // SOLUCIÓN: El hambre S_HUNGER ya no dispara masticación imaginaria.
    // Solo alimenta el deseo en el globo [🍎] y la tristeza si baja de 25%.
    synapses.push_back(Synapse(S_FATIGUE, M_SLEEP, 1.2f));
    synapses.push_back(Synapse(S_PAIN,    M_SICK,  1.5f));
    synapses.push_back(Synapse(S_TOUCH,   M_PLAY,  0.6f)); 
    synapses.push_back(Synapse(S_TOUCH,   M_IDLE,  0.2f));
}

void Brain::emitReward(float amount) {
    dopamine = constrain(dopamine + amount, 0.0f, 2.0f);
    if (amount > 0.4f) {
        currentThought = THOUGHT_HEART; 
    }
}

void Brain::onFed() {
    neurons[S_HUNGER].potential = 0.0f; // Sacia el potencial eléctrico de hambre
    driveHunger = 0.0f;
}

void Brain::processPlasticity(float dt) {
    float safeDt = min(dt, 0.5f);
    dopamine = 0.2f + (dopamine - 0.2f) * expf(-0.1f * safeDt);
    if (isnan(dopamine) || dopamine < 0.0f) dopamine = 0.2f;

    for (size_t i = 0; i < synapses.size(); ++i) {
        if (synapses[i].preIndex == S_TOUCH && synapses[i].postIndex == M_PLAY) {
            if (neurons[S_TOUCH].didSpike && neurons[M_PLAY].didSpike) {
                float rewardSignal = dopamine * 0.08f;
                synapses[i].reinforce(0.04f, rewardSignal);
            }
            synapses[i].weight -= (synapses[i].weight - 0.5f) * 0.0005f * safeDt;
        }
    }
}

void Brain::evaluateThoughts(const PetSensoryInput& input) {
    driveHunger   = 100.0f - constrain(input.hunger, 0.0f, 100.0f);
    driveSleep    = 100.0f - constrain(input.energy, 0.0f, 100.0f);
    driveSocial   = 100.0f - constrain(input.happiness, 0.0f, 100.0f);
    driveDistress = (100.0f - constrain(input.health, 0.0f, 100.0f)) + (constrain(input.poopCount, 0, 3) * 25.0f);

    ThoughtType newThought = THOUGHT_NONE;
    String motivo = "Mente en calma";

    if (input.health < 40.0f) {
        newThought = THOUGHT_MED;
        motivo = "Salud baja (" + String((int)input.health) + "%). Necesita medicina.";
    }
    else if (input.poopCount > 0) {
        newThought = THOUGHT_POOP;
        motivo = "Incomodidad: Hay " + String(input.poopCount) + " caca(s) en el suelo.";
    }
    else if (driveHunger >= 40.0f) {
        newThought = THOUGHT_FOOD;
        motivo = "Apetito. Deseo de comer alimento.";
    }
    else if (driveSleep >= 60.0f && !input.lightsOn) {
        newThought = THOUGHT_SLEEP;
        motivo = "Deseo de dormir. Energia baja.";
    }
    else if (driveSocial >= 45.0f) {
        newThought = THOUGHT_PLAY;
        motivo = "Aburrimiento. Deseo de jugar.";
    }
    else if (dopamine > 0.8f) {
        newThought = THOUGHT_HEART;
        motivo = "Placer quimico por afecto recibido.";
    }
    else if (random(0, 100) < 5 && currentThought == THOUGHT_NONE) {
        newThought = THOUGHT_CURIOUS;
        motivo = "Curiosidad espontanea / Explorando entorno.";
    }

    currentThought = newThought;

    if (currentThought != lastLoggedThought && currentThought != THOUGHT_NONE) {
        lastLoggedThought = currentThought;
        Serial.println("\n--------------------------------------------------");
        Serial.printf("[MIND] Nuevo pensamiento en globo: ID %d\n", (int)currentThought);
        Serial.printf("[MIND] Motivo neuronal: %s\n", motivo.c_str());
        Serial.printf("[MIND] Impulsos -> Hambre: %.0f%% | Social: %.0f%% | Fatiga: %.0f%% | Malestar: %.0f%%\n",
                      driveHunger, driveSocial, driveSleep, driveDistress);
        Serial.printf("[MIND] Estado corporal: %s (El cuerpo permanece estable)\n", 
                      (currentDecision == PetState::Idle ? "Idle" : "Activo"));
        Serial.println("--------------------------------------------------");
    }
}

void Brain::update(float dt, const PetSensoryInput& input) {
    float safeDt = min(dt, 0.5f);

    neurons[S_HUNGER].feed((100.0f - constrain(input.hunger, 0.0f, 100.0f)) * 0.05f * safeDt);
    neurons[S_FATIGUE].feed((100.0f - constrain(input.energy, 0.0f, 100.0f)) * 0.05f * safeDt);
    neurons[S_PAIN].feed(((100.0f - constrain(input.health, 0.0f, 100.0f)) * 0.05f + (constrain(input.poopCount, 0, 3) * 0.5f)) * safeDt);

    if (input.touched) {
        neurons[S_TOUCH].feed(4.0f * safeDt); 
    }

    // Remembranza espontánea (Solo si está feliz > 60 y en reposo)
    memoryRecallTimer += safeDt;
    if (memoryRecallTimer >= nextRecallInterval) {
        memoryRecallTimer = 0.0f;
        nextRecallInterval = random(30, 80); 
        
        float affectionWeight = synapses[2].weight; // S_TOUCH -> M_PLAY
        if (affectionWeight > 1.2f && input.happiness > 60.0f && currentDecision == PetState::Idle) {
            Serial.printf("[BRAIN] ¡Recuerdo espontaneo de afecto! Peso: %.2f\n", affectionWeight);
            neurons[M_PLAY].feed(neurons[M_PLAY].threshold); 
        }
    }

    for (int i = 0; i < 5; ++i) {
        if (neurons[i].update(safeDt)) {
            globalSpikeCount++;
            for (size_t s = 0; s < synapses.size(); ++s) {
                if (synapses[s].preIndex == i) {
                    if (synapses[s].postIndex == M_PLAY && input.happiness < 35.0f) continue;
                    neurons[synapses[s].postIndex].feed(synapses[s].weight);
                }
            }
        }
    }

    for (int i = 5; i < 9; ++i) {
        if (neurons[i].update(safeDt)) {
            globalSpikeCount++;
        }
    }

    processPlasticity(safeDt);

    spikeTimer += safeDt;
    if (spikeTimer >= 1.0f) {
        spikeRate = (float)globalSpikeCount / spikeTimer;
        globalSpikeCount = 0;
        spikeTimer = 0.0f;
    }

    // 1. Evaluar el pensamiento en el globo
    evaluateThoughts(input);

    // 2. Si hay una acción deliberada activa (salto de caricia)
    if (actionDurationTimer > 0.0f) {
        actionDurationTimer -= safeDt;
        return; 
    }

    // 3. Estado físico: Sueño en oscuridad
    if (!input.lightsOn) {
        if (input.energy >= 100.0f) {
            currentDecision = PetState::Idle; 
        } else {
            currentDecision = PetState::Sleeping; 
        }
        return;
    }

    if (currentDecision == PetState::Sleeping) {
        currentDecision = PetState::Idle;
    }

    // 4. Estado físico: Enfermedad sostenida
    if (input.health < 40.0f || input.poopCount >= 2) {
        currentDecision = PetState::Sick;
        return;
    }

    // 5. Estado físico: Hambre crítica (< 25%) -> Debilidad/tristeza
    if (input.hunger < 25.0f) {
        currentDecision = PetState::Sad;
        return;
    }

    // 6. Estado físico: Tristeza sostenida por abandono
    if (input.happiness < 25.0f) {
        currentDecision = PetState::Sad;
        return;
    }

    // 7. Ráfaga motora de alegría (solo por caricias o minijuego)
    if (neurons[M_PLAY].didSpike && input.happiness >= 35.0f) {
        currentDecision = PetState::Happy;
        actionDurationTimer = 2.5f; 
    } else {
        // SOLUCIÓN: Reposo pacífico por defecto. NUNCA se decide Eating aquí.
        currentDecision = PetState::Idle; 
    }
}

PetState Brain::getDecision() const {
    return currentDecision;
}

float Brain::getStressLevel() const {
    return neurons[S_PAIN].potential;
}
