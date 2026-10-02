// src/tamagotchi/PetScene.h
#pragma once
#include "../render/Scene.h"
#include "../render/Sprite.h"
#include "Pet.h"
#include "ClockWidget.h"
#include "StatusHUD.h"
#include "ThoughtBubble.h"
#include "EvolutionFX.h"
#include <memory>

class PetScene : public Scene {
public:
    PetScene(Pet& petInstance);

    void enter() override;
    void exit() override;
    void update(float dt) override;

    void onTouch(uint16_t x, uint16_t y);
    void onTouchReleased();
    void startMinigame();

private:
    Pet& pet;
    
    std::shared_ptr<Sprite> petSprite = nullptr;
    std::shared_ptr<Sprite> oldPetSprite = nullptr; 

    std::shared_ptr<Sprite> btnFood = nullptr;
    std::shared_ptr<Sprite> btnMed = nullptr;
    std::shared_ptr<Sprite> btnClean = nullptr;
    std::shared_ptr<Sprite> btnLamp = nullptr;

    std::shared_ptr<Sprite> poopSprites[3];
    
    std::shared_ptr<ClockWidget> clockWidget = nullptr;
    std::shared_ptr<StatusHUD> statusHUD = nullptr; 
    std::shared_ptr<ThoughtBubble> thoughtBubble = nullptr; 

    // Capas visuales de la evolución
    std::shared_ptr<EvolutionVortex>  evolutionVortex = nullptr;  // Capa 1: Fondo detrás del bicho
    std::shared_ptr<EvolutionOverlay> evolutionOverlay = nullptr; // Capa 5: Fundido y Diálogo delante

    String currentTexturePath = "";
    PetStage lastKnownStage = PetStage::Egg;

    // Estados de la cinemática
    bool isEvolving = false;
    float evolutionTimer = 0.0f;
    float flickerTimer = 0.0f;
    float currentFlickerInterval = 0.35f;
    bool showNewStage = false;
    int silhouetteColorToggle = 0;

    float animTimer = 0.0f;
    uint8_t currentFrame = 0;
    uint8_t totalFrames = 1;

    int activePressedButton = -1;
    float buttonPressFeedbackTimer = 0.0f;

    uint16_t prevPetTouchX = 0;
    float accumulatedStroke = 0.0f;
    float petCooldownTimer = 0.0f;

    // Minijuego
    bool isMinigameActive = false;
    int minigameRound = 0;
    int minigameScore = 0;
    float minigameTimer = 0.0f;
    int petChoice = 0; 
    bool waitingPlayerChoice = false;

    void updateSpriteTexture();
    void startEvolutionSequence(PetStage oldStage);
    void processEvolutionSequence(float dt);
    void processMinigame(float dt);
};
