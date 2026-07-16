// src/render/BaseScenes.h
#pragma once
#include "Scene.h"
#include "Sprite.h"
#include "../assets/AssetManager.h"
#include <Arduino.h>

// En src/render/BaseScenes.h (Actualizar variables y lógica del huevo)
class SplashScene : public Scene {
public:
    void enter() override {
        // 1. Cargar la mascota
        auto petTex = AssetManager::getInstance().getTexture("/tama/sprites/base/tiernito/huevo/idle.png");
        if (petTex) {
            petSprite = std::make_shared<Sprite>(petTex);
            petSprite->frameSize = {48, 48};
            petSprite->frameOffset = {0, 0};

            int16_t frameW = 48 * 3;
            int16_t frameH = 48 * 3;
            petSprite->position = { (int16_t)((172 - frameW) / 2), (int16_t)((320 - frameH) / 2) };
            petSprite->scale = {3, 3}; 
            petSprite->layer = RenderLayer::Characters;
            addObject(petSprite);

            // --- CÁLCULO DINÁMICO DE FOTOGRAMAS ---
            // Si mide 96px de ancho, totalPetFrames = 2. Si mide 192px, totalPetFrames = 4.
            totalPetFrames = petTex->width / 48; 
            Serial.printf("[KERNEL] Animación adaptativa cargada: %d fotogramas detectados.\n", totalPetFrames);
        }

        // 2. Cargar el icono de hambre
        auto iconTex = AssetManager::getInstance().getTexture("/tama/ui/hunger_icon.png");
        if (iconTex) {
            auto iconSprite = std::make_shared<Sprite>(iconTex);
            iconSprite->position = {16, 16};
            iconSprite->layer = RenderLayer::UI;
            addObject(iconSprite);
        }

        animTimer = 0.0f;
        currentFrame = 0;
    }

    void exit() override {
        clearObjects();
        petSprite = nullptr;
    }

    void update(float dt) override {
        // Ciclar usando el número de frames dinámico calculado al arrancar
        if (petSprite && totalPetFrames > 0) {
            animTimer += dt;
            if (animTimer >= 0.25f) {
                animTimer = 0.0f;
                currentFrame = (currentFrame + 1) % totalPetFrames; // Cicla de acuerdo al total real
                petSprite->frameOffset.x = currentFrame * 48;
            }
        }
    }

private:
    std::shared_ptr<Sprite> petSprite = nullptr;
    float animTimer = 0.0f;
    uint8_t currentFrame = 0;
    uint8_t totalPetFrames = 0; // Almacena el número dinámico de fotogramas
};
