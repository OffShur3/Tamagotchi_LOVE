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
    lastThoughtReason = "Mente en reposo";
    forcedThoughtTimer = 0.0f;
    dopamine = 0.5f;
    spikeRate = 0.0f;
    globalSpikeCount = 0;
    spikeTimer = 0.0f;
    actionDurationTimer = 0.0f;
    memoryRecallTimer = 0.0f;
    nextRecallInterval = 30.0f;

    ruminationLevel = 0.0f;
    timeSinceLastTouch = 0.0f;
    hungerMetabolicMult = 1.0f;
    energyDrainMult = 1.0f;
    stressMult = 1.0f;
    isEggStage = false;
}

void Brain::setupConnectome() {
    // 1. Umbrales y fugas calibrados: M_IDLE actúa como atractor ganador en equilibrio
    neurons[S_HUNGER].threshold  = 1.0f;  neurons[S_HUNGER].leak  = 0.5f;
    neurons[S_FATIGUE].threshold = 1.0f;  neurons[S_FATIGUE].leak = 0.5f;
    neurons[S_BOREDOM].threshold = 1.0f;  neurons[S_BOREDOM].leak = 0.5f;
    neurons[S_PAIN].threshold    = 1.0f;  neurons[S_PAIN].leak    = 0.6f; 
    neurons[S_TOUCH].threshold   = 0.5f;  neurons[S_TOUCH].leak   = 0.8f; 

    neurons[M_SLEEP].threshold   = 2.0f;  neurons[M_SLEEP].leak   = 0.5f; 
    neurons[M_PLAY].threshold    = 1.8f;  neurons[M_PLAY].leak    = 0.6f; 
    neurons[M_SICK].threshold    = 1.8f;  neurons[M_SICK].leak    = 0.5f; // Mayor umbral para evitar falsos enfermos
    neurons[M_IDLE].threshold    = 0.8f;  neurons[M_IDLE].leak    = 0.4f; // Atractor basal por defecto

    synapses.clear();

    // 2. Proyecciones Sensoriales Excitatorias hacia la corteza motora
    synapses.push_back(Synapse(S_FATIGUE, M_SLEEP,  1.2f));
    synapses.push_back(Synapse(S_PAIN,    M_SICK,   0.9f)); // Sensibilidad moderada (requiere dolor real)
    synapses.push_back(Synapse(S_TOUCH,   M_PLAY,   0.8f)); // Plástica / Hebbiana
    synapses.push_back(Synapse(S_TOUCH,   M_IDLE,   0.4f));
    synapses.push_back(Synapse(S_BOREDOM, M_PLAY,   1.0f));
    synapses.push_back(Synapse(S_HUNGER,  M_IDLE,   0.5f));

    // 3. Inhibición Cruzada Sensorial: El afecto mitiga el dolor directamente
    synapses.push_back(Synapse(S_TOUCH,   S_PAIN,  -0.30f));

    // 4. Inhibición de Calma por Dolor Severo (Moderada para no inducir colapso)
    synapses.push_back(Synapse(S_PAIN,    M_IDLE,  -0.25f));

    // 5. Inhibición Lateral Motora Recalibrada (Winner-Take-All suave: entre -0.25 y -0.40)
    synapses.push_back(Synapse(M_SLEEP,   M_PLAY,  -0.30f));
    synapses.push_back(Synapse(M_PLAY,    M_SLEEP, -0.30f));
    synapses.push_back(Synapse(M_SICK,    M_PLAY,  -0.35f));
    synapses.push_back(Synapse(M_PLAY,    M_SICK,  -0.35f));
    synapses.push_back(Synapse(M_SLEEP,   M_IDLE,  -0.30f));
    synapses.push_back(Synapse(M_IDLE,    M_SLEEP, -0.25f));

    // 6. Inercia Emocional / Auto-recurrencia
    synapses.push_back(Synapse(M_PLAY,    M_PLAY,   0.25f));
    synapses.push_back(Synapse(M_SLEEP,   M_SLEEP,  0.30f));
    synapses.push_back(Synapse(M_IDLE,    M_IDLE,   0.20f)); // Refuerzo de estabilidad en calma
}

void Brain::emitReward(float amount) {
    if (isEggStage) return;
    dopamine = constrain(dopamine + amount, 0.0f, 2.0f);
    if (amount > 0.4f) {
        nudgeThought(THOUGHT_HEART, "Afecto directo recibido (+dopamina)", 0.4f);
    }
}

void Brain::onFed() {
    neurons[S_HUNGER].potential = 0.0f;
    driveHunger = 0.0f;
    if (currentThought == THOUGHT_FOOD) {
        ruminationLevel = 0.0f;
        currentThought = THOUGHT_NONE;
    }
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

String Brain::getActiveThoughtName() const {
    if (isEggStage) return "Inconsciente (Huevo)";
    switch (currentThought) {
        case THOUGHT_HEART:        return "Amor / Gratitud";
        case THOUGHT_FOOD:         return "Hambre / Apetito";
        case THOUGHT_PLAY:         return "Juego / Social";
        case THOUGHT_SLEEP:        return "Sueno / Cansancio";
        case THOUGHT_POOP:         return "Incomodidad (Caca)";
        case THOUGHT_MED:          return "Dolor / Medicina";
        case THOUGHT_STRESS:       return "Estres / Molestia";
        case THOUGHT_CURIOUS:      return "Curiosidad";
        case THOUGHT_NOSTALGIA:    return "Nostalgia (Reloj)";
        case THOUGHT_ANTICIPATION: return "Anticipacion / Espera";
        case THOUGHT_CAPRICE:      return "Capricho / Hastio";
        default:                   return "En calma";
    }
}

void Brain::stimulateNeuron(int idx, float amount) {
    if (isEggStage || idx < 0 || idx >= (int)neurons.size()) return;

    neurons[idx].feed(amount);

    if (idx == S_PAIN) {
        driveDistress = constrain(driveDistress + amount * 20.0f, 0.0f, 100.0f);
        nudgeThought(THOUGHT_STRESS, "Estimulo de dolor (S_PAIN)", amount * 0.35f);
    } else if (idx == S_HUNGER) {
        driveHunger = constrain(driveHunger + amount * 20.0f, 0.0f, 100.0f);
        nudgeThought(THOUGHT_FOOD, "Estimulo de apetito (S_HUNGER)", amount * 0.35f);
    } else if (idx == S_FATIGUE) {
        driveSleep = constrain(driveSleep + amount * 20.0f, 0.0f, 100.0f);
        nudgeThought(THOUGHT_SLEEP, "Estimulo de fatiga (S_FATIGUE)", amount * 0.35f);
    } else if (idx == S_BOREDOM) {
        driveSocial = constrain(driveSocial + amount * 20.0f, 0.0f, 100.0f);
        nudgeThought(THOUGHT_PLAY, "Estimulo de aburrimiento (S_BOREDOM)", amount * 0.35f);
    } else if (idx == S_TOUCH) {
        dopamine = constrain(dopamine + 0.3f, 0.0f, 2.0f);
        nudgeThought(THOUGHT_HEART, "Estimulo de afecto (S_TOUCH)", amount * 0.35f);
    }
}

void Brain::nudgeThought(ThoughtType t, const String& reason, float nudge) {
    if (isEggStage) return;

    if (currentThought == t) {
        // En sucesión rápida sobre el mismo pensamiento, la intensidad se acumula y acelera exponencialmente
        ruminationLevel = (ruminationLevel * 1.3f) + nudge;
    } else {
        // Cambio de pensamiento inicia en el nivel base
        currentThought = t;
        ruminationLevel = nudge;
    }

    if (ruminationLevel > 2.5f) ruminationLevel = 2.5f;

    lastThoughtReason = reason + " [Rumiacion: " + String(ruminationLevel, 2) + "]";
    forcedThoughtTimer = 3.5f; // La burbuja se hace visible de inmediato

    // Desbordamiento Somático: solo por encima de 1.0f se altera el cuerpo/sprite
    if (ruminationLevel > 1.0f) {
        overflowThoughtToMotor(t);
    }
}

void Brain::overflowThoughtToMotor(ThoughtType t) {
    if (t == THOUGHT_STRESS || t == THOUGHT_MED) {
        currentDecision = PetState::Sick;
        actionDurationTimer = 3.5f;
        driveDistress = max(driveDistress, 80.0f);
    } else if (t == THOUGHT_FOOD) {
        currentDecision = PetState::Sad;
        actionDurationTimer = 2.5f;
        driveHunger = max(driveHunger, 75.0f);
    } else if (t == THOUGHT_PLAY) {
        currentDecision = PetState::Sad;
        actionDurationTimer = 2.5f;
        driveSocial = max(driveSocial, 75.0f);
    } else if (t == THOUGHT_HEART) {
        currentDecision = PetState::Happy;
        actionDurationTimer = 3.0f;
        dopamine = max(dopamine, 1.2f);
    }
}

void Brain::evaluateThoughts(const PetSensoryInput& input) {
    if (input.isEgg) {
        currentThought = THOUGHT_NONE;
        ruminationLevel = 0.0f;
        lastThoughtReason = "Inconsciente (Etapa de Huevo)";
        return;
    }

    if (input.health <= 0.0f) {
        currentThought = THOUGHT_NONE;
        lastThoughtReason = "Actividad cerebral cesada (Fallecido)";
        driveDistress = 100.0f;
        driveHunger = 0.0f;
        driveSleep = 0.0f;
        driveSocial = 0.0f;
        dopamine = 0.0f;
        spikeRate = 0.0f;
        currentDecision = PetState::Dead;
        ruminationLevel = 0.0f;
        return;
    }

    driveHunger = 100.0f - constrain(input.hunger, 0.0f, 100.0f);
    driveSleep  = 100.0f - constrain(input.energy, 0.0f, 100.0f);
    driveSocial = 100.0f - constrain(input.happiness, 0.0f, 100.0f);
    
    float baseDistress = (100.0f - constrain(input.health, 0.0f, 100.0f)) + (constrain(input.poopCount, 0, 3) * 25.0f);
    float painSensory = constrain(neurons[S_PAIN].potential * 35.0f, 0.0f, 100.0f);
    driveDistress = constrain(max(baseDistress, painSensory), 0.0f, 100.0f);

    ThoughtType newThought = THOUGHT_NONE;
    String motivo = "Mente en reposo";

    // 1. Dolor agudo
    if (input.health < 25.0f || input.poopCount >= 3 || driveDistress >= 65.0f) {
        newThought = THOUGHT_STRESS;
        motivo = "Estres y malestar agudo (" + String((int)driveDistress) + "% malestar).";
    }
    // 2. Necesidad de medicina
    else if (input.health < 45.0f || driveDistress >= 40.0f) {
        newThought = THOUGHT_MED;
        motivo = "Salud deteriorada (" + String((int)input.health) + "%). Requiere botiquin.";
    }
    // 3. Incomodidad por suciedad
    else if (input.poopCount > 0) {
        newThought = THOUGHT_POOP;
        motivo = "Incomodidad: Hay " + String(input.poopCount) + " caca(s) en el entorno.";
    }
    // 4. Apetito
    else if (driveHunger >= 40.0f) {
        newThought = THOUGHT_FOOD;
        motivo = "Apetito. Impulso de hambre al " + String((int)driveHunger) + "%.";
    }
    // 5. Cansancio con luz apagada
    else if (driveSleep >= 60.0f && !input.lightsOn) {
        newThought = THOUGHT_SLEEP;
        motivo = "Deseo de dormir. Energia baja (" + String((int)input.energy) + "%).";
    }
    // 6. Aburrimiento
    else if (driveSocial >= 45.0f) {
        newThought = THOUGHT_PLAY;
        motivo = "Aburrimiento. Deseo social al " + String((int)driveSocial) + "%.";
    }
    // 7. Afecto
    else if (dopamine > 0.8f) {
        newThought = THOUGHT_HEART;
        motivo = "Placer por caricia recibida (Dopamina: " + String(dopamine, 2) + ").";
    }
    // 8. Nostalgia (Reloj): Homeostasis plena (>80%) y mente en calma
    else if (input.hunger > 80.0f && input.energy > 80.0f && input.happiness > 80.0f && input.health > 80.0f && input.poopCount == 0) {
        newThought = THOUGHT_NOSTALGIA;
        motivo = "Ensonacion nostálgica: Mirando las horas pasar en paz.";
    }
    // 9. Anticipación: Espera atenta de contacto
    else if (timeSinceLastTouch > 90.0f && input.happiness >= 45.0f && input.health >= 50.0f) {
        newThought = THOUGHT_ANTICIPATION;
        motivo = "Anticipacion: " + String((int)timeSinceLastTouch) + "s esperando caricias.";
    }
    // 10. Capricho / Hastío leve
    else if (input.hunger > 60.0f && input.energy > 50.0f && input.happiness < 60.0f) {
        newThought = THOUGHT_CAPRICE;
        motivo = "Capricho: Necesidades cubiertas, buscando novedad.";
    }
    // 11. Curiosidad espontánea
    else if (random(0, 100) < 5 && currentThought == THOUGHT_NONE) {
        newThought = THOUGHT_CURIOUS;
        motivo = "Curiosidad espontanea explorando entorno.";
    }

    if (newThought != THOUGHT_NONE) {
        if (newThought == currentThought) {
            ruminationLevel = min(1.0f, ruminationLevel + 0.05f);
        } else {
            ruminationLevel = 0.20f;
        }
    }

    currentThought = newThought;
    lastThoughtReason = motivo;
}

void Brain::computePsychosomaticMultipliers() {
    if (isEggStage) {
        hungerMetabolicMult = 1.0f;
        energyDrainMult = 1.0f;
        stressMult = 1.0f;
        return;
    }

    if (currentThought == THOUGHT_FOOD) {
        hungerMetabolicMult = 1.0f + (min(ruminationLevel, 1.0f) * 0.65f);
    } else if (currentThought == THOUGHT_NOSTALGIA || currentThought == THOUGHT_HEART) {
        hungerMetabolicMult = 0.85f;
    } else if (currentThought == THOUGHT_STRESS) {
        hungerMetabolicMult = 1.0f + (min(ruminationLevel, 1.0f) * 0.35f);
    } else {
        hungerMetabolicMult = 1.0f;
    }

    if (currentThought == THOUGHT_STRESS || currentThought == THOUGHT_MED) {
        energyDrainMult = 1.0f + (neurons[S_PAIN].potential * 0.25f) + (min(ruminationLevel, 1.0f) * 0.40f);
        stressMult = 1.0f + (min(ruminationLevel, 1.0f) * 0.50f);
    } else if (currentThought == THOUGHT_CAPRICE) {
        energyDrainMult = 1.0f + (min(ruminationLevel, 1.0f) * 0.20f);
        stressMult = 1.0f;
    } else if (currentThought == THOUGHT_NOSTALGIA) {
        energyDrainMult = 0.80f;
        stressMult = 0.75f;
    } else {
        energyDrainMult = 1.0f;
        stressMult = 1.0f;
    }
}

void Brain::update(float dt, const PetSensoryInput& input) {
    float safeDt = min(dt, 0.5f);
    isEggStage = input.isEgg;

    // SILENCIO COGNITIVO TOTAL EN ETAPA DE HUEVO
    if (isEggStage) {
        currentThought = THOUGHT_NONE;
        ruminationLevel = 0.0f;
        forcedThoughtTimer = 0.0f;
        lastThoughtReason = "Inconsciente (Etapa de Huevo)";
        currentDecision = PetState::Idle;
        hungerMetabolicMult = 1.0f;
        energyDrainMult = 1.0f;
        stressMult = 1.0f;
        for (auto& n : neurons) {
            n.potential = 0.0f;
            n.didSpike = false;
        }
        return;
    }

    if (input.health <= 0.0f) {
        evaluateThoughts(input);
        computePsychosomaticMultipliers();
        return;
    }

    if (input.touched) {
        timeSinceLastTouch = 0.0f;
    } else {
        timeSinceLastTouch += safeDt;
    }

    // Decaimiento natural y orgánico de la rumiación cuando no hay nuevos impulsos
    if (forcedThoughtTimer > 0.0f) {
        forcedThoughtTimer -= safeDt;
    } else {
        ruminationLevel = max(0.0f, ruminationLevel - (0.20f * safeDt));
        if (ruminationLevel <= 0.0f && currentThought != THOUGHT_NONE) {
            currentThought = THOUGHT_NONE;
        }
    }

    // Alimentación de neuronas sensoriales (0..4)
    neurons[S_HUNGER].feed((100.0f - constrain(input.hunger, 0.0f, 100.0f)) * 0.04f * safeDt);
    neurons[S_FATIGUE].feed((100.0f - constrain(input.energy, 0.0f, 100.0f)) * 0.04f * safeDt);
    neurons[S_PAIN].feed(((100.0f - constrain(input.health, 0.0f, 100.0f)) * 0.05f + (constrain(input.poopCount, 0, 3) * 0.35f)) * safeDt);
    neurons[S_BOREDOM].feed((100.0f - constrain(input.happiness, 0.0f, 100.0f)) * 0.04f * safeDt);

    if (input.touched) {
        neurons[S_TOUCH].feed(3.0f * safeDt); 
    }

    // Recuerdo espontáneo
    memoryRecallTimer += safeDt;
    if (memoryRecallTimer >= nextRecallInterval) {
        memoryRecallTimer = 0.0f;
        nextRecallInterval = random(30, 80); 
        
        float affectionWeight = 0.8f;
        for (const auto& syn : synapses) {
            if (syn.preIndex == S_TOUCH && syn.postIndex == M_PLAY) {
                affectionWeight = syn.weight;
                break;
            }
        }

        if (affectionWeight > 1.2f && input.happiness > 60.0f && currentDecision == PetState::Idle) {
            neurons[M_PLAY].feed(neurons[M_PLAY].threshold); 
        }
    }

    // Paso 1: Actualización de neuronas sensoriales (0..4)
    for (int i = 0; i < 5; ++i) {
        if (neurons[i].update(safeDt)) {
            globalSpikeCount++;
            for (size_t s = 0; s < synapses.size(); ++s) {
                if (synapses[s].preIndex == i) {
                    if (synapses[s].postIndex == M_PLAY && input.happiness < 25.0f) continue;
                    neurons[synapses[s].postIndex].feed(synapses[s].weight);
                }
            }
        }
    }

    // Paso 2: Actualización de neuronas motoras (5..8)
    for (int i = 5; i < 9; ++i) {
        if (neurons[i].update(safeDt)) {
            globalSpikeCount++;
            for (size_t s = 0; s < synapses.size(); ++s) {
                if (synapses[s].preIndex == i) {
                    neurons[synapses[s].postIndex].feed(synapses[s].weight);
                }
            }
        }
    }

    processPlasticity(safeDt);

    spikeTimer += safeDt;
    if (spikeTimer >= 1.0f) {
        spikeRate = (float)globalSpikeCount / spikeTimer;
        globalSpikeCount = 0;
        spikeTimer = 0.0f;
    }

    if (forcedThoughtTimer <= 0.0f) {
        evaluateThoughts(input);
    }

    computePsychosomaticMultipliers();

    if (actionDurationTimer > 0.0f) {
        actionDurationTimer -= safeDt;
        return; 
    }

    if (!input.lightsOn) {
        currentDecision = (input.energy >= 100.0f) ? PetState::Idle : PetState::Sleeping;
        return;
    }

    if (currentDecision == PetState::Sleeping) {
        currentDecision = PetState::Idle;
    }

    // Evaluación motora Winner-Take-All
    if (neurons[M_SICK].didSpike) {
        currentDecision = PetState::Sick;
        actionDurationTimer = 3.0f;
        return;
    }

    if (neurons[M_PLAY].didSpike && input.happiness >= 35.0f) {
        currentDecision = PetState::Happy;
        actionDurationTimer = 2.5f; 
        return;
    }

    // Desbordamiento Somático: solo si las pulsiones son críticas o la rumiación supera el umbral supra (>1.0)
    if (input.health < 25.0f || input.poopCount >= 2 || (driveDistress >= 60.0f && ruminationLevel >= 1.0f)) {
        currentDecision = PetState::Sick;
        return;
    }

    if (input.hunger < 15.0f || input.happiness < 15.0f || 
       ((input.hunger < 30.0f || input.happiness < 30.0f) && ruminationLevel >= 1.0f)) {
        currentDecision = PetState::Sad;
        return;
    }

    // Pensamiento subumbral: Mantiene el sprite físico en reposo (Idle)
    currentDecision = PetState::Idle; 
}

PetState Brain::getDecision() const {
    return currentDecision;
}

float Brain::getStressLevel() const {
    return neurons[S_PAIN].potential;
}

String Brain::getTelemetryJson() const {
    String json = "{";
    json += "\"name\":\"" + String(getName()) + "\",";
    json += "\"dopamine\":" + String(dopamine, 2) + ",";
    json += "\"spikeRate\":" + String(spikeRate, 1) + ",";
    json += "\"thought\":" + String((int)currentThought) + ",";
    json += "\"thoughtName\":\"" + getActiveThoughtName() + "\",";
    json += "\"thoughtReason\":\"" + lastThoughtReason + "\",";
    json += "\"rumination\":" + String(ruminationLevel, 2) + ",";
    json += "\"hungerMult\":" + String(hungerMetabolicMult, 2) + ",";
    json += "\"energyMult\":" + String(energyDrainMult, 2) + ",";
    json += "\"stressMult\":" + String(stressMult, 2) + ",";
    
    // Impulsos
    json += "\"drives\":{";
    json += "\"hunger\":" + String(driveHunger, 1) + ",";
    json += "\"sleep\":" + String(driveSleep, 1) + ",";
    json += "\"social\":" + String(driveSocial, 1) + ",";
    json += "\"distress\":" + String(driveDistress, 1);
    json += "},";

    // Neuronas
    json += "\"neurons\":[";
    for (size_t i = 0; i < neurons.size(); ++i) {
        json += "{\"v\":" + String(neurons[i].potential, 2) + ",";
        json += "\"th\":" + String(neurons[i].threshold, 2) + ",";
        json += "\"spk\":" + String(neurons[i].didSpike ? "true" : "false") + "}";
        if (i < neurons.size() - 1) json += ",";
    }
    json += "],";

    // Sinapsis
    json += "\"synapses\":[";
    for (size_t i = 0; i < synapses.size(); ++i) {
        json += "{\"pre\":" + String(synapses[i].preIndex) + ",";
        json += "\"post\":" + String(synapses[i].postIndex) + ",";
        json += "\"w\":" + String(synapses[i].weight, 2) + "}";
        if (i < synapses.size() - 1) json += ",";
    }
    json += "]}";
    return json;
}
