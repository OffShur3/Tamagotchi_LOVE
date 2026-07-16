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
    
    // 1. Limpiar el framebuffer de trabajo inicialmente con negro
    renderer.clear(0x0000);

    bool backgroundBaked = false;

    // 2. CARGA MOMENTÁNEA: Solicitar la textura del fondo de pantalla
    auto bgTex = AssetManager::getInstance().getTexture("/tama/ui/bg_main.png");
    if (bgTex) {
        if (renderer.hasBackgroundBuffer()) {
            // Modo PSRAM: Pintar el fondo de forma estática en la capa de abajo
            renderer.drawToBackground(bgTex, 0, 0);
            
            // Liberación inmediata de RAM
            bgTex = nullptr;
            AssetManager::getInstance().clearUnused();
            backgroundBaked = true;
            Serial.println("[KERNEL] PSRAM Detectada: Fondo bajeado estáticamente y liberado de la RAM.");
        } else {
            // Modo SRAM (Tu placa): Pintar el fondo directamente en el framebuffer de trabajo una sola vez
            renderer.drawToFramebufferOnce(bgTex, 0, 0);
            
            // --- INVERSIÓN DE FLUJO CRÍTICA ---
            // Liberamos la textura pesada del fondo de la RAM PRIMERO para vaciar el Heap antes de alojar el respaldo
            bgTex = nullptr;
            AssetManager::getInstance().clearUnused();
            Serial.println("[KERNEL] Fondo pintado una vez. Textura desalojada de RAM de forma inmediata.");
            
            // Calcular la posición exacta donde se centrará la mascota (14, 88)
            int16_t petW = 48 * 3;
            int16_t petH = 48 * 3;
            int16_t petX = (172 - petW) / 2;
            int16_t petY = (320 - petH) / 2;
            
            // Ahora que la RAM interna está libre y limpia, guardamos el respaldo de 41 KB de forma exitosa
            renderer.savePetBackingStore(petX, petY, petW, petH);
        }
    } else {
        Serial.println("[KERNEL] No se pudo cargar el fondo de pantalla. Usando fondo negro por defecto.");
    }

    // 3. Arrancar la escena dinámica (La escena ya no carga el fondo en absoluto)
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

    // Actualizar animaciones
    SceneManager::getInstance().update(dt);

    // Renderizar escena activa
    auto currentScene = SceneManager::getInstance().getCurrentScene();
    if (currentScene) {
        renderer.render(currentScene->getObjects());
    }
}

void Game::flush(Arduino_GFX* display) {
    if (!display) return;
    display->draw16bitRGBBitmap(0, 0, renderer.getFramebuffer(), renderer.getWidth(), renderer.getHeight());
}
