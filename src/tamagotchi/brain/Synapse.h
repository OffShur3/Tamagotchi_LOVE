// src/tamagotchi/brain/Synapse.h
#pragma once
#include <Arduino.h>

struct Synapse {
    int preIndex;
    int postIndex;
    float weight;
    float maxWeight;
    float minWeight;

    // Constructor por defecto con soporte de pesos con signo [-2.0, 2.0]
    Synapse() 
        : preIndex(0), postIndex(0), weight(1.0f), maxWeight(2.0f), minWeight(-2.0f) {}

    // Constructor formal con soporte de sinapsis inhibitorias negativas
    Synapse(int pre, int post, float w, float maxW = 2.0f, float minW = -2.0f)
        : preIndex(pre), postIndex(post), weight(w), maxWeight(maxW), minWeight(minW) {}

    // Modulación por plasticidad / recompensa dentro de los límites con signo
    void reinforce(float learningRate, float reward) {
        weight += learningRate * reward;
        if (weight > maxWeight) weight = maxWeight;
        if (weight < minWeight) weight = minWeight;
    }
};
