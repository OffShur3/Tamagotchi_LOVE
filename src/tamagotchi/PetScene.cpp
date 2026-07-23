#include "PetScene.h"
#include "../assets/AssetManager.h"
#include <Arduino.h>

PetScene::PetScene(Pet& petRef) : pet(petRef) {}

void PetScene::enter() {
    // 1. Cargar Widget de Reloj estilo Stardew Valley
    clockWidget = std::make_shared<ClockWidget>();
    addObject(clockWidget);

    // 2. Cargar Botonera Inferior (Icons UI Spritesheet)
    auto uiTex = AssetManager::getInstance().getTexture("/tama/ui/icons_ui.png");
    if (uiTex) {
        // Botón 1: Comida (10, 275)
        btnFood = std::make_shared<Sprite>(uiTex);
        btnFood->frameSize = {16, 16};
        btnFood->frameOffset = {16, 0}; // Coordenada Comida
        btnFood->position = {10, 275};
        btnFood->scale = {2, 2}; // 32x32 píxeles táctil
        btnFood->layer = RenderLayer::UI;
        addObject(btnFood);

        // Botón 2: Medicina (52, 275)
        btnMed = std::make_shared<Sprite>(uiTex);
        btnMed->frameSize = {16, 16};
        btnMed->frameOffset = {0, 0}; // Coordenada Botiquín
        btnMed->position = {52, 275};
        btnMed->scale = {2, 2};
        btnMed->layer = RenderLayer::UI;
        addObject(btnMed);

        // Botón 3: Limpiar (94, 275)
        btnClean = std::make_shared<Sprite>(uiTex);
        btnClean->frameSize = {16, 16};
        btnClean->frameOffset = {0, 16}; // Coordenada Escoba
        btnClean->position = {94, 275};
        btnClean->scale = {2, 2};
        btnClean->layer = RenderLayer::UI;
        addObject(btnClean);

        // Botón 4: Luz (136, 275)
        btnLamp = std::make_shared<Sprite>(uiTex);
        btnLamp->frameSize = {16, 16};
        btnLamp->frameOffset = {16, 16}; // Lámpara encendida por defecto
        btnLamp->position = {136, 275};
        btnLamp->scale = {2, 2};
        btnLamp->layer = RenderLayer::UI;
        addObject(btnLamp);
    }

    // 2. Cargar Objetos de Entorno (Caca / Alimento)
    auto envTex = AssetManager::getInstance().getTexture("/tama/ui/objects_env.png");
    if (envTex) {
        poopSprite = std::make_shared<Sprite>(envTex);
        poopSprite->frameSize = {16, 16};
        poopSprite->frameOffset = {16, 0}; // Coordenada Caca
        poopSprite->position = {130, 210};
        poopSprite->scale = {2, 2};
        poopSprite->visible = false;
        poopSprite->layer = RenderLayer::World;
        addObject(poopSprite);
    }

    updateSpriteTexture();
}

void PetScene::exit() {
    clearObjects();
    petSprite = nullptr;
    btnFood = btnMed = btnClean = btnLamp = poopSprite = nullptr;
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
    updateSpriteTexture();

    // Mostrar u ocultar la caca en el suelo
    if (poopSprite) {
        poopSprite->visible = (pet.getPoopCount() > 0);
    }

    // Cambiar ícono de lámpara ON/OFF
    if (btnLamp) {
        btnLamp->frameOffset.x = pet.isLightOn() ? 16 : 32;
    }

    // Animación de la mascota
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
    // Zona de Botonera Inferior (Y >= 260)
    if (y >= 260) {
        if (x >= 5 && x <= 45) { pet.feed(); }
        else if (x >= 46 && x <= 85) { pet.heal(); }
        else if (x >= 86 && x <= 125) { pet.clean(); }
        else if (x >= 126 && x <= 165) { pet.toggleLights(); }
        return;
    }

    // Tocar a la mascota directamente (Centro)
    if (x >= 20 && x <= 150 && y >= 80 && y <= 240) {
        pet.pet();
    }
}
