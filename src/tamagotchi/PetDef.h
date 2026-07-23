#pragma once
#include <Arduino.h>

enum class PetStage {
    Egg,       // Huevo
    Baby,      // Bebé
    Child,     // Niño
    Adult,     // Adulto
    Senior,    // Anciano
    Dead       // Muerto
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

// Funciones auxiliares para mapear las carpetas físicas en la SD
inline String stageToString(PetStage stage) {
    switch (stage) {
        case PetStage::Egg:    return "huevo";
        case PetStage::Baby:   return "bebe";
        case PetStage::Child:  return "child";
        case PetStage::Adult:  return "adulto";
        case PetStage::Senior: return "anciano";
        case PetStage::Dead:   return "huevo";
    }
    return "huevo";
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
