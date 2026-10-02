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
        potential += input;
        if (potential > 10.0f) potential = 10.0f; // Evitar saturación
        if (potential < -5.0f) potential = -5.0f;
    }

    bool update(float dt) {
        didSpike = false;
        
        // SOLUCIÓN MATEMÁTICA: Decaimiento exponencial incondicionalmente estable
        float safeDt = min(dt, 0.5f);
        potential = restPotential + (potential - restPotential) * expf(-leak * safeDt);

        if (isnan(potential)) potential = restPotential;
        
        if (potential >= threshold) {
            potential = restPotential;
            didSpike = true;
            lastSpikeTime = millis();
        }
        return didSpike;
    }
};
