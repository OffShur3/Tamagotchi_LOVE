// src/tamagotchi/brain/Neuron.h
#pragma once
#include <Arduino.h>
#include <math.h>

struct Neuron {
    float potential;
    float threshold;
    float leak;
    float restPotential;
    uint32_t lastSpikeTime;
    bool didSpike;

    Neuron() 
        : potential(0.0f), threshold(1.0f), leak(0.5f), 
          restPotential(0.0f), lastSpikeTime(0), didSpike(false) {}

    void feed(float input) {
        if (isnan(input) || isinf(input)) return;
        potential += input;
        
        // Límite de despolarización
        if (potential > 10.0f) potential = 10.0f;
        
        // PISO BIOLÓGICO ESTRICTO: Previene el abismo hiperpolarizante inalcanzable
        if (potential < -0.4f) potential = -0.4f;
    }

    bool update(float dt) {
        didSpike = false;
        
        // Decaimiento exponencial incondicionalmente estable hacia restPotential (0.0f)
        float safeDt = min(dt, 0.5f);
        potential = restPotential + (potential - restPotential) * expf(-leak * safeDt);

        if (isnan(potential) || isinf(potential)) {
            potential = restPotential;
        }

        // Asegurar que la relajación nunca perfore el piso de membrana
        if (potential < -0.4f) {
            potential = -0.4f;
        }
        
        if (potential >= threshold) {
            potential = restPotential;
            didSpike = true;
            lastSpikeTime = millis();
        }
        return didSpike;
    }
};
