// src/Game.cpp
#include "Game.h"
#include "render/SceneManager.h"
#include "render/BaseScenes.h"
#include "assets/AssetManager.h"
#include <Arduino.h>

Game::Game(uint16_t width, uint16_t height) 
    : renderer(width, height), lastFrameTime(0) {}

void Game::init() {
    lastFrameTime = millis();
    renderer.clear(0x0000);

    bool backgroundBaked = false;

    if (renderer.hasBackgroundBuffer()) {
        auto bgTex = AssetManager::getInstance().getTexture("/tama/ui/bg_main.png");
        if (bgTex) {
            renderer.drawToBackground(bgTex, 0, 0);
            bgTex = nullptr;
            AssetManager::getInstance().clearUnused();
            backgroundBaked = true;
        }
    } else {
        // --- TÉCNICA ZERO-COPY (SOLUCIÓN A LA MEMORIA) ---
        // Le pasamos el puntero directo de la pantalla. El AssetManager va a 
        // pintar los pixeles directamente ahí mientras lee la SD, sin gastar RAM extra.
        uint16_t* fb = renderer.getFramebuffer();
        if (fb && AssetManager::getInstance().loadPNGDirectToBuffer("/tama/ui/bg_main.png", fb)) {
            backgroundBaked = true;
            Serial.println("[KERNEL] MODO SRAM: Fondo cargado directo a memoria sin copias extras.");
            
            // Guardamos el "Backing Store" para la mascota.
            int16_t petW = 48 * 3;
            int16_t petH = 48 * 3;
            int16_t petX = (172 - petW) / 2;
            int16_t petY = (320 - petH) / 2;
            
            renderer.savePetBackingStore(petX, petY, petW, petH);
        }
    }

    if (!backgroundBaked) {
        Serial.println("[KERNEL] No se pudo cargar el fondo de pantalla. Usando fondo negro por defecto.");
    }

    auto splash = std::make_shared<SplashScene>();
    SceneManager::getInstance().changeScene(splash);
}

void Game::handleInput() {
    // Lectura del touch sensor
}

void Game::tick() {
    uint32_t currentTime = millis();
    float dt = (currentTime - lastFrameTime) / 1000.0f;
    lastFrameTime = currentTime;

    handleInput();

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

// --- IMPLEMENTACIONES POR DEFECTO PARA EL ORQUESTADOR ---
void Game::saveGame() {
    Serial.println("[GAME] Guardado de partida no implementado en la aplicacion activa.");
}

void Game::resetGame() {
    Serial.println("[GAME] Reinicio de partida no implementado en la aplicacion activa.");
}

void Game::redraw() {
    // El renderizado es continuo en el loop, no requiere accion especial en el kernel base
}
