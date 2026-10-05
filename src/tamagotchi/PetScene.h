// src/tamagotchi/PetScene.h
#pragma once
#include "../render/Scene.h"
#include "../render/Sprite.h"
#include "Pet.h"
#include "ClockWidget.h"
#include "StatusHUD.h"
#include "ThoughtBubble.h"
#include "EvolutionFX.h"
#include "MessagePopup.h"
#include "MessageManager.h"
#include <memory>
#include <vector>

class PetScene : public Scene {
public:
    PetScene(Pet& petInstance);

    void enter() override;
    void exit() override;
    void update(float dt) override;

    void onTouch(uint16_t x, uint16_t y);
    void onTouchReleased();

    void startMinigame();
    void startAnimationTest();

    bool hasDeathTouch() const { return deathTouchTriggered; }
    void clearDeathTouch() { deathTouchTriggered = false; }

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

    std::shared_ptr<EvolutionVortex>  evolutionVortex = nullptr;  
    std::shared_ptr<EvolutionOverlay> evolutionOverlay = nullptr; 

    std::shared_ptr<MessagePopup> messagePopup = nullptr;
    float telemetryBroadcastTimer = 0.0f;
    float spontaneousThoughtTimer = 0.0f;
    float dreamTimer = 0.0f;
    float nextDreamInterval = 45.0f;

    String currentTexturePath = "";
    PetStage lastKnownStage = PetStage::Egg;

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

    bool deathTouchTriggered = false;

    // Minijuego
    bool isMinigameActive = false;
    int minigameRound = 0;
    int minigameScore = 0;
    float minigameTimer = 0.0f;
    int petChoice = 0; 
    bool waitingPlayerChoice = false;

    // Showcase de animaciones
    struct AnimEntry {
        PetStage stage;
        String animName;
        String path;
    };
    std::vector<AnimEntry> availableTestAnims;
    size_t animTestIndex = 0;
    float animTestTimer = 0.0f;
    bool isAnimTestActive = false;

    void updateSpriteTexture();
    void startEvolutionSequence(PetStage oldStage);
    void processEvolutionSequence(float dt);
    void processMinigame(float dt);
    void processAnimationTest(float dt);
    void loadTestAnimation();
};
