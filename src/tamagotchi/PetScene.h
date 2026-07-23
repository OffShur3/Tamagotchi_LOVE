#pragma once
#include "../render/Scene.h"
#include "../render/Sprite.h"
#include "Pet.h"
#include "ClockWidget.h" // <-- NUEVO
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
    
    std::shared_ptr<Sprite> petSprite = nullptr;
    std::shared_ptr<Sprite> hungerIcon = nullptr;

    std::shared_ptr<Sprite> btnFood = nullptr;
    std::shared_ptr<Sprite> btnMed = nullptr;
    std::shared_ptr<Sprite> btnClean = nullptr;
    std::shared_ptr<Sprite> btnLamp = nullptr;

    std::shared_ptr<Sprite> poopSprite = nullptr;
    std::shared_ptr<ClockWidget> clockWidget = nullptr; // <-- NUEVO

    String currentTexturePath = "";
    float animTimer = 0.0f;
    uint8_t currentFrame = 0;
    uint8_t totalFrames = 1;

    void updateSpriteTexture();
};
