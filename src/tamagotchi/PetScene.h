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
#include "ConfigHelpModal.h"
#include <memory>
#include <vector>
#include <functional>

// Widget de botón con acabado metálico de acero / cromo pulido de alto contraste
class GearButtonWidget : public RenderObject {
public:
    GearButtonWidget() {
        layer = RenderLayer::UI;
        position = {142, 30}; // Debajo del reloj
    }

    Size2 getSize() const override { return {14, 14}; }

    void draw(RenderContext& ctx) override {
        if (!visible) return;

        int16_t gx = position.x;
        int16_t gy = position.y;

        uint16_t cBlack     = 0x0000;
        uint16_t cHighlight = 0xFFFF;
        uint16_t cSilver    = 0xD69A;
        uint16_t cSteel     = 0x94B2;
        uint16_t cAxle      = 0x18C3;

        static const uint8_t gear14[14][14] = {
            {0,0,0,0,1,1,1,1,0,0,0,0,0,0},
            {0,0,0,1,2,2,3,1,0,0,0,0,0,0},
            {0,1,1,1,2,2,3,1,1,1,0,0,0,0},
            {0,1,2,2,2,3,3,3,3,1,1,0,0,0},
            {1,2,2,1,1,5,5,1,1,3,3,1,0,0},
            {1,2,1,1,5,5,5,5,1,1,3,1,0,0},
            {1,2,1,5,5,5,5,5,5,1,3,1,0,0},
            {1,2,1,5,5,5,5,5,5,1,3,1,0,0},
            {1,2,1,1,5,5,5,5,1,1,3,1,0,0},
            {1,2,2,1,1,5,5,1,1,3,3,1,0,0},
            {0,1,2,2,2,3,3,3,3,1,1,0,0,0},
            {0,1,1,1,2,2,3,1,1,1,0,0,0,0},
            {0,0,0,1,2,2,3,1,0,0,0,0,0,0},
            {0,0,0,0,1,1,1,1,0,0,0,0,0,0}
        };

        for (int y = 0; y < 14; ++y) {
            int16_t py = gy + y;
            if (py < 0 || py >= ctx.height) continue;
            for (int x = 0; x < 14; ++x) {
                int16_t px = gx + x;
                if (px < 0 || px >= ctx.width) continue;

                uint8_t pixel = gear14[y][x];
                if (pixel == 1) ctx.framebuffer[py * ctx.width + px] = cBlack;
                else if (pixel == 2) ctx.framebuffer[py * ctx.width + px] = cHighlight;
                else if (pixel == 3) ctx.framebuffer[py * ctx.width + px] = cSilver;
                else if (pixel == 4) ctx.framebuffer[py * ctx.width + px] = cSteel;
                else if (pixel == 5) ctx.framebuffer[py * ctx.width + px] = cAxle;
            }
        }
    }
};

class PetScene : public Scene {
public:
    using EditPlayerNameCallback = std::function<void()>;

    PetScene(Pet& petInstance);

    void enter() override;
    void exit() override;
    void update(float dt) override;

    void onTouch(uint16_t x, uint16_t y) override;
    void onTouchReleased() override;

    void setOnEditPlayerName(EditPlayerNameCallback cb) {
        onEditPlayerName = cb;
    }

    void startMinigame();
    void startAnimationTest();

    bool hasDeathTouch() const { return deathTouchTriggered; }
    void clearDeathTouch() { deathTouchTriggered = false; }
    bool isConfigModalActive() const { return configHelpModal && configHelpModal->isVisible(); }

    // <--- NUEVO MÉTODO: Fuerza la recarga de la textura actual de la mascota
    void forceTextureUpdate() {
        currentTexturePath = ""; 
        updateSpriteTexture();
    }

private:
    Pet& pet;
    EditPlayerNameCallback onEditPlayerName = nullptr;
    
    std::shared_ptr<Sprite> petSprite = nullptr;
    std::shared_ptr<Sprite> oldPetSprite = nullptr; 

    std::shared_ptr<Sprite> btnFood = nullptr;
    std::shared_ptr<Sprite> btnMed = nullptr;
    std::shared_ptr<Sprite> btnClean = nullptr;
    std::shared_ptr<Sprite> btnLamp = nullptr;

    std::shared_ptr<Sprite> poopSprites[3];
    
    std::shared_ptr<ClockWidget> clockWidget = nullptr;
    std::shared_ptr<GearButtonWidget> gearWidget = nullptr;
    std::shared_ptr<StatusHUD> statusHUD = nullptr; 
    std::shared_ptr<ThoughtBubble> thoughtBubble = nullptr; 

    std::shared_ptr<EvolutionVortex>  evolutionVortex = nullptr;  
    std::shared_ptr<EvolutionOverlay> evolutionOverlay = nullptr; 

    std::shared_ptr<MessagePopup> messagePopup = nullptr;
    std::shared_ptr<ConfigHelpModal> configHelpModal = nullptr;

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
