// src/tamagotchi/brain/Synapse.h
#pragma once
#include <Arduino.h>

struct Synapse {
    int preIndex;
    int postIndex;
    float weight;
    float maxWeight;
    float minWeight;

    // Constructor por defecto
    Synapse() 
        : preIndex(0), postIndex(0), weight(1.0f), maxWeight(2.0f), minWeight(0.1f) {}

    // Constructor formal compatible con GCC 8.4 (C++11 / C++14 / C++17)
    Synapse(int pre, int post, float w, float maxW = 2.0f, float minW = 0.1f)
        : preIndex(pre), postIndex(post), weight(w), maxWeight(maxW), minWeight(minW) {}

    // Modulación por recompensa Hebbiana
    void reinforce(float learningRate, float reward) {
        weight += learningRate * reward;
        if (weight > maxWeight) weight = maxWeight;
        if (weight < minWeight) weight = minWeight;
    }
};
