// src/Game.cpp
#include "Game.h"
#include "render/SceneManager.h"
#include "assets/AssetManager.h"
#include "core/touch_axs5106.h"
#include "core/PlayerManager.h"
#include <Arduino.h>

Game::Game(uint16_t width, uint16_t height) 
    : renderer(width, height), lastFrameTime(0) {}

void Game::init() {
    lastFrameTime = millis();
    renderer.clear(0x0000);

    if (renderer.hasBackgroundBuffer()) {
        auto bgTex = AssetManager::getInstance().getTexture("/tama/ui/bg_main.png");
        if (bgTex) {
            renderer.drawToBackground(bgTex, 0, 0);
            bgTex = nullptr;
            AssetManager::getInstance().clearUnused();
        }
    } else {
        uint16_t* fb = renderer.getFramebuffer();
        if (fb && AssetManager::getInstance().loadPNGDirectToBuffer("/tama/ui/bg_main.png", fb)) {
            int16_t petW = 48 * 3;
            int16_t petH = 48 * 3;
            renderer.savePetBackingStore((172 - petW) / 2, (320 - petH) / 2, petW, petH);
        }
    }

    pet.init();
    petScene = std::make_shared<PetScene>(pet);

    // Cableado para edición de nombre desde la pestaña SIS del modal
    petScene->setOnEditPlayerName([this]() {
        Serial.println("[KERNEL] Solicitud de cambio de nombre recibida. Abriendo selector en modo edición...");
        nameSelectScene = std::make_shared<NameSelectScene>([this]() {
            Serial.println("[ONBOARDING] Nombre actualizado con éxito. Regresando a PetScene...");
            SceneManager::getInstance().changeScene(petScene);
            nameSelectScene = nullptr;
        }, PlayerManager::getInstance().getOwnerName());

        SceneManager::getInstance().changeScene(nameSelectScene);
    });

    // Verificación de Primer Arranque (Onboarding)
    if (!PlayerManager::getInstance().hasProfile()) {
        Serial.println("[KERNEL] Sin perfil previo: Iniciando Onboarding en NameSelectScene...");
        nameSelectScene = std::make_shared<NameSelectScene>([this]() {
            Serial.println("[ONBOARDING] Nombre confirmado. Transicionando a PetScene...");
            SceneManager::getInstance().changeScene(petScene);
            nameSelectScene = nullptr;
        }, "");
        SceneManager::getInstance().changeScene(nameSelectScene);
    } else {
        Serial.printf("[KERNEL] Bienvenido de nuevo, \"%s\". Iniciando mascota...\n", 
                      PlayerManager::getInstance().getOwnerName().c_str());
        SceneManager::getInstance().changeScene(petScene);
    }
}

void Game::handleInput() {
    uint16_t tx = 0, ty = 0;
    auto curScene = SceneManager::getInstance().getCurrentScene();

    if (touch_read(tx, ty)) {
        if (curScene) {
            curScene->onTouch(tx, ty);
        }
    } else {
        if (curScene) {
            curScene->onTouchReleased();
        }
    }
}

void Game::tick() {
    uint32_t currentTime = millis();
    float dt = (currentTime - lastFrameTime) / 1000.0f;
    lastFrameTime = currentTime;

    if (dt > 0.2f) dt = 0.2f;

    handleInput();

    // Solo actualizar la mascota y el ciclo noche si no estamos en el selector de nombre
    if (!nameSelectScene) {
        pet.update(dt);
        renderer.setNightMode(!pet.isLightOn());
    } else {
        renderer.setNightMode(false);
    }

    SceneManager::getInstance().update(dt);

    auto currentScene = SceneManager::getInstance().getCurrentScene();
    if (currentScene) {
        renderer.render(currentScene->getObjects());
    }
}

void Game::flush(Arduino_GFX* display) {
    if (!display) return;
    display->draw16bitRGBBitmap(0, 0, renderer.getFramebuffer(), renderer.getWidth(), renderer.getHeight());
}

void Game::saveGame() { pet.save(); }
void Game::resetGame() { pet.reset(); }
void Game::redraw() {}
void Game::printStats() const { pet.printStats(); }
void Game::onTimeSynced() { pet.catchUpTime(); }
