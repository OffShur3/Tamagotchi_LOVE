#pragma once
#include "../render/Scene.h"
#include "../render/Sprite.h"
#include "Pet.h"
#include "ClockWidget.h"
#include <memory>

class PetScene : public Scene {
public:
    PetScene(Pet& petInstance);

    void enter() override;
    void exit() override;
    void update(float dt) override;

    void onTouch(uint16_t x, uint16_t y);

private:
    Pet& pet;
    
    // Sprites principales
    std::shared_ptr<Sprite> petSprite = nullptr;
    std::shared_ptr<Sprite> oldPetSprite = nullptr; // Guardado temporal para el efecto GBA
    std::shared_ptr<Sprite> hungerIcon = nullptr;

    // Botones de interfaz
    std::shared_ptr<Sprite> btnFood = nullptr;
    std::shared_ptr<Sprite> btnMed = nullptr;
    std::shared_ptr<Sprite> btnClean = nullptr;
    std::shared_ptr<Sprite> btnLamp = nullptr;

    // Elementos de entorno
    std::shared_ptr<Sprite> poopSprite = nullptr;
    std::shared_ptr<ClockWidget> clockWidget = nullptr;

    String currentTexturePath = "";
    PetStage lastKnownStage = PetStage::Egg;

    // --- VARIABLES DE LA ANIMACIÓN DE EVOLUCIÓN GBA ---
    bool isEvolving = false;
    float evolutionTimer = 0.0f;
    float flickerTimer = 0.0f;
    float currentFlickerInterval = 0.4f;
    bool showNewStage = false;

    float animTimer = 0.0f;
    uint8_t currentFrame = 0;
    uint8_t totalFrames = 1;

    void updateSpriteTexture();
    void startEvolutionSequence(PetStage oldStage);
    void processEvolutionSequence(float dt);
};
