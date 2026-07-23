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

    // Carga de Fondo de Pantalla Zero-Copy
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

    // Inicializar Mascota y Cargar Escena de Juego
    pet.init();
    petScene = std::make_shared<PetScene>(pet);
    SceneManager::getInstance().changeScene(petScene);
}

void Game::handleInput() {
    uint16_t tx = 0, ty = 0;
    if (touch_is_pressed()) {
        touch_get_xy(&tx, &ty);
        
        // Mapeo directo de la pantalla táctil invertida
        uint16_t mapX = (tx > 172) ? 0 : (172 - tx);
        uint16_t mapY = ty;

        if (petScene) {
            petScene->onTouch(mapX, mapY);
        }
    }
}

void Game::tick() {
    uint32_t currentTime = millis();
    float dt = (currentTime - lastFrameTime) / 1000.0f;
    lastFrameTime = currentTime;

    handleInput();

    // Actualizar simulación de la mascota
    pet.update(dt);

    // Actualizar gráfico de la escena actual
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

void Game::saveGame() {
    pet.save();
}

void Game::resetGame() {
    pet.reset();
}

void Game::redraw() {
    // Redibujado en loop continuo
}

void Game::printStats() const {
    pet.printStats();
}
void Game::onTimeSynced() {
    pet.catchUpTime(); // Notifica a la mascota que recalcule su estado con la hora real
}
