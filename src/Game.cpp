#include "Game.h"
#include "render/SceneManager.h"
#include "assets/AssetManager.h"
#include "core/touch_axs5106.h"
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
    SceneManager::getInstance().changeScene(petScene);
}

void Game::handleInput() {
    uint16_t tx = 0, ty = 0;
    // LECTURA TÁCTIL UNIFICADA
    if (touch_read(tx, ty)) {
        if (petScene) {
            petScene->onTouch(tx, ty);
        }
    }
}

void Game::tick() {
    uint32_t currentTime = millis();
    float dt = (currentTime - lastFrameTime) / 1000.0f;
    lastFrameTime = currentTime;

    if (dt > 0.2f) dt = 0.2f; // Protección contra picos de tiempo

    handleInput();

    pet.update(dt);
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
