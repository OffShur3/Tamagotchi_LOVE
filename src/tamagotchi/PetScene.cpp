#include "PetScene.h"
#include "../assets/AssetManager.h"
#include <Arduino.h>

PetScene::PetScene(Pet& petRef) : pet(petRef) {}

void PetScene::enter() {
    // 1. Reloj de madera estilo Stardew
    clockWidget = std::make_shared<ClockWidget>();
    addObject(clockWidget);

    // 2. Botonera inferior
    auto uiTex = AssetManager::getInstance().getTexture("/tama/ui/icons_ui.png");
    if (uiTex) {
        btnFood = std::make_shared<Sprite>(uiTex);
        btnFood->frameSize = {16, 16}; btnFood->frameOffset = {16, 0};
        btnFood->position = {10, 275}; btnFood->scale = {2, 2};
        btnFood->layer = RenderLayer::UI;
        addObject(btnFood);

        btnMed = std::make_shared<Sprite>(uiTex);
        btnMed->frameSize = {16, 16}; btnMed->frameOffset = {0, 0};
        btnMed->position = {52, 275}; btnMed->scale = {2, 2};
        btnMed->layer = RenderLayer::UI;
        addObject(btnMed);

        btnClean = std::make_shared<Sprite>(uiTex);
        btnClean->frameSize = {16, 16}; btnClean->frameOffset = {0, 16};
        btnClean->position = {94, 275}; btnClean->scale = {2, 2};
        btnClean->layer = RenderLayer::UI;
        addObject(btnClean);

        btnLamp = std::make_shared<Sprite>(uiTex);
        btnLamp->frameSize = {16, 16}; btnLamp->frameOffset = {16, 16};
        btnLamp->position = {136, 275}; btnLamp->scale = {2, 2};
        btnLamp->layer = RenderLayer::UI;
        addObject(btnLamp);
    }

    // 3. Caca en el suelo
    auto envTex = AssetManager::getInstance().getTexture("/tama/ui/objects_env.png");
    if (envTex) {
        poopSprite = std::make_shared<Sprite>(envTex);
        poopSprite->frameSize = {16, 16}; poopSprite->frameOffset = {16, 0};
        poopSprite->position = {130, 210}; poopSprite->scale = {2, 2};
        poopSprite->visible = false; poopSprite->layer = RenderLayer::World;
        addObject(poopSprite);
    }

    lastKnownStage = pet.getStage();
    updateSpriteTexture();
}

void PetScene::exit() {
    clearObjects();
    petSprite = oldPetSprite = hungerIcon = nullptr;
    btnFood = btnMed = btnClean = btnLamp = poopSprite = nullptr;
    clockWidget = nullptr;
}

void PetScene::startEvolutionSequence(PetStage oldStage) {
    isEvolving = true;
    evolutionTimer = 0.0f;
    flickerTimer = 0.0f;
    currentFlickerInterval = 0.4f;
    showNewStage = false;

    // Congelar el sprite antiguo
    oldPetSprite = petSprite;

    // Cargar el sprite de la nueva etapa
    petSprite = nullptr;
    currentTexturePath = "";
    updateSpriteTexture();

    if (oldPetSprite && petSprite) {
        oldPetSprite->visible = true;
        petSprite->visible = false;
    }

    Serial.println("[SCENE] ¡Iniciando secuencia de evolución estilo Pokémon GBA!");
}

void PetScene::processEvolutionSequence(float dt) {
    evolutionTimer += dt;
    flickerTimer += dt;

    // Aceleración matemática del parpadeo: baja de 400ms a 30ms en 4 segundos
    currentFlickerInterval = max(0.03f, 0.4f - (evolutionTimer / 4.0f) * 0.37f);

    if (flickerTimer >= currentFlickerInterval) {
        flickerTimer = 0.0f;
        showNewStage = !showNewStage;

        if (oldPetSprite && petSprite) {
            oldPetSprite->visible = !showNewStage;
            petSprite->visible = showNewStage;
        }
    }

    // Finalización de la secuencia (a los 4 segundos)
    if (evolutionTimer >= 4.0f) {
        isEvolving = false;
        if (oldPetSprite) {
            removeObject(oldPetSprite);
            oldPetSprite = nullptr;
        }
        if (petSprite) {
            petSprite->visible = true;
        }
        Serial.println("[SCENE] ¡Evolución finalizada con éxito!");
    }
}

void PetScene::updateSpriteTexture() {
    String newPath = pet.getSpritePath();
    if (newPath == currentTexturePath && petSprite != nullptr) return;

    currentTexturePath = newPath;
    auto tex = AssetManager::getInstance().getTexture(newPath.c_str());

    if (tex) {
        if (!petSprite) {
            petSprite = std::make_shared<Sprite>(tex);
            petSprite->layer = RenderLayer::Characters;
            petSprite->scale = {3, 3}; 
            addObject(petSprite);
        } else {
            petSprite->texture = tex;
        }

        petSprite->frameSize = {48, 48};
        petSprite->frameOffset = {0, 0};
        petSprite->position = { (int16_t)((172 - 144) / 2), (int16_t)((320 - 144) / 2) };

        totalFrames = max(1, tex->width / 48);
        currentFrame = 0;
    }
}

void PetScene::update(float dt) {
    // 1. Detección automática de Evolución
    if (!isEvolving && pet.getStage() != lastKnownStage) {
        PetStage oldStage = lastKnownStage;
        lastKnownStage = pet.getStage();
        startEvolutionSequence(oldStage);
    }

    // 2. Si está evolucionando, ejecutar la animación GBA y pausar el resto
    if (isEvolving) {
        processEvolutionSequence(dt);
        return; 
    }

    updateSpriteTexture();

    if (poopSprite) {
        poopSprite->visible = (pet.getPoopCount() > 0);
    }

    if (btnLamp) {
        btnLamp->frameOffset.x = pet.isLightOn() ? 16 : 32;
    }

    if (petSprite && totalFrames > 1) {
        animTimer += dt;
        if (animTimer >= 0.25f) {
            animTimer = 0.0f;
            currentFrame = (currentFrame + 1) % totalFrames;
            petSprite->frameOffset.x = currentFrame * 48;
        }
    }
}

void PetScene::onTouch(uint16_t x, uint16_t y) {
    // Bloquear controles táctiles durante el espectáculo de evolución
    if (isEvolving) return; 

    if (y >= 260) {
        if (x >= 5 && x <= 45) { pet.feed(); }
        else if (x >= 46 && x <= 85) { pet.heal(); }
        else if (x >= 86 && x <= 125) { pet.clean(); }
        else if (x >= 126 && x <= 165) { pet.toggleLights(); }
        return;
    }

    if (x >= 20 && x <= 150 && y >= 80 && y <= 240) {
        pet.pet();
    }
}
