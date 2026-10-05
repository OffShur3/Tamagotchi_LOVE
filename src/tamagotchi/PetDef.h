// src/tamagotchi/PetDef.h
#pragma once
#include <Arduino.h>

enum class PetStage {
    Egg,       // Huevo
    Baby,      // Bebé
    Child,     // Niño
    Adult,     // Adulto
    Senior,    // Anciano
    Dead       // Estado terminal
};

enum class PetState {
    Idle,
    Eating,
    Sleeping,
    Sick,
    Sad,
    Happy,
    Dead
};

inline String stageToString(PetStage stage) {
    switch (stage) {
        case PetStage::Egg:    return "huevo";
        case PetStage::Baby:   return "bebe";
        case PetStage::Child:  return "child";
        case PetStage::Adult:  return "adulto";
        case PetStage::Senior: return "anciano";
        case PetStage::Dead:   return "bebe"; // Fallback seguro
    }
    return "bebe";
}

inline String stateToString(PetState state) {
    switch (state) {
        case PetState::Idle:     return "idle";
        case PetState::Eating:   return "eating";
        case PetState::Sleeping: return "sleeping";
        case PetState::Sick:     return "sick";
        case PetState::Sad:      return "sad";
        case PetState::Happy:    return "happy";
        case PetState::Dead:     return "dead";
    }
    return "idle";
}
