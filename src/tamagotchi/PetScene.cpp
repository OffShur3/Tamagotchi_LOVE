// src/tamagotchi/PetScene.cpp
#include "PetScene.h"
#include "../assets/AssetManager.h"
#include <Arduino.h>

PetScene::PetScene(Pet& petRef) : pet(petRef) {}

void PetScene::enter() {
    clockWidget = std::make_shared<ClockWidget>();
    addObject(clockWidget);

    statusHUD = std::make_shared<StatusHUD>(pet);
    addObject(statusHUD);

    thoughtBubble = std::make_shared<ThoughtBubble>(pet);
    addObject(thoughtBubble);

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

    auto envTex = AssetManager::getInstance().getTexture("/tama/ui/objects_env.png");
    if (envTex) {
        int16_t poopPositions[3][2] = { {128, 210}, {12, 212}, {134, 235} };
        for (int i = 0; i < 3; i++) {
            poopSprites[i] = std::make_shared<Sprite>(envTex);
            poopSprites[i]->frameSize = {16, 16};
            poopSprites[i]->frameOffset = {16, 0};
            poopSprites[i]->position = {poopPositions[i][0], poopPositions[i][1]};
            poopSprites[i]->scale = {2, 2};
            poopSprites[i]->visible = false;
            poopSprites[i]->layer = RenderLayer::World;
            addObject(poopSprites[i]);
        }
    }

    lastKnownStage = pet.getStage();
    updateSpriteTexture();
}

void PetScene::exit() {
    clearObjects();
    petSprite = oldPetSprite = nullptr;
    btnFood = btnMed = btnClean = btnLamp = nullptr;
    for (int i = 0; i < 3; i++) poopSprites[i] = nullptr;
    clockWidget = nullptr;
    statusHUD = nullptr;
    thoughtBubble = nullptr;
    evolutionVortex = nullptr;
    evolutionOverlay = nullptr;
}

// INICIAR SECUENCIA GBA CON CAPAS SEPARADAS
void PetScene::startEvolutionSequence(PetStage oldStage) {
    isEvolving = true;
    evolutionTimer = 0.0f;
    flickerTimer = 0.0f;
    currentFlickerInterval = 0.35f;
    showNewStage = false;
    silhouetteColorToggle = 0;

    // 1. Ocultar widgets secundarios
    if (clockWidget)   clockWidget->visible = false;
    if (statusHUD)     statusHUD->visible = false;
    if (thoughtBubble) thoughtBubble->visible = false;
    if (btnFood)       btnFood->visible = false;
    if (btnMed)        btnMed->visible = false;
    if (btnClean)      btnClean->visible = false;
    if (btnLamp)       btnLamp->visible = false;
    for (int i = 0; i < 3; i++) {
        if (poopSprites[i]) poopSprites[i]->visible = false;
    }

    // 2. Capa 1 (World): Vórtice que tapa el fondo 100% (Detrás del bicho)
    evolutionVortex = std::make_shared<EvolutionVortex>();
    evolutionVortex->visible = false; // Comienza oculto hasta que termine el fundido a blanco
    addObject(evolutionVortex);

    // 3. Capa 5 (Popup): Fundido blanco en 5 pasos y cuadro de texto (Delante de todo)
    evolutionOverlay = std::make_shared<EvolutionOverlay>();
    evolutionOverlay->setStageNames(stageToString(oldStage), stageToString(pet.getStage()));
    evolutionOverlay->setFadeStep(0);
    addObject(evolutionOverlay);

    // 4. Preparar sprites de las dos etapas (Capa 2: Characters)
    oldPetSprite = petSprite;
    if (oldPetSprite) {
        oldPetSprite->silhouetteMode = false; // Comienza en color real hasta que la pantalla se vuelva blanca
        oldPetSprite->visible = true;
    }

    petSprite = nullptr;
    currentTexturePath = "";
    updateSpriteTexture();

    if (petSprite) {
        petSprite->silhouetteMode = true;
        petSprite->silhouetteColor = 0xFFFF;
        petSprite->visible = false;
    }

    Serial.println("\n[EVOLUCIÓN GBA] ¡Iniciando fundido blanco y alternancia de siluetas!");
}

// CINEMÁTICA EN 5 FASES
void PetScene::processEvolutionSequence(float dt) {
    evolutionTimer += dt;

    if (evolutionVortex)  evolutionVortex->update(dt);
    if (evolutionOverlay) evolutionOverlay->update(dt);

    // FASE 1: FUNDIDO A BLANCO EN 5 PASOS (0.0s a 0.35s)
    if (evolutionTimer < 0.35f) {
        // Mapear el tiempo en 5 pasos (0 a 5)
        uint8_t step = (uint8_t)constrain((evolutionTimer / 0.35f) * 5.0f, 0.0f, 5.0f);
        if (evolutionOverlay) {
            evolutionOverlay->setFadeStep(step);
            evolutionOverlay->setDialogueActive(false, false);
        }
    }
    // FASE 2: APERTURA DE LA ARENA DESDE BLANCO (0.35s a 0.70s)
    else if (evolutionTimer >= 0.35f && evolutionTimer < 0.70f) {
        // La pantalla está totalmente blanca. Cambiamos el fondo al vórtice oscuro y activamos la silueta
        if (evolutionVortex) evolutionVortex->visible = true;
        if (oldPetSprite) {
            oldPetSprite->silhouetteMode = true;
            oldPetSprite->silhouetteColor = 0xFFFF;
        }

        // Desvanecer el blanco en 5 pasos inversos (5 a 0)
        float progress = (evolutionTimer - 0.35f) / 0.35f;
        uint8_t step = 5 - (uint8_t)constrain(progress * 5.0f, 0.0f, 5.0f);
        if (evolutionOverlay) {
            evolutionOverlay->setFadeStep(step);
            evolutionOverlay->setDialogueActive(true, false); // Mostrar "¿Que? ¡Esta evolucionando!"
        }
    }
    // FASE 3: ALTERNANCIA ACELERADA DE SILUETAS EN EL VÓRTICE (0.70s a 4.2s)
    else if (evolutionTimer >= 0.70f && evolutionTimer < 4.2f) {
        if (evolutionOverlay) evolutionOverlay->setFadeStep(0); // Sin velo blanco

        flickerTimer += dt;
        // Aceleración de 350ms a 35ms
        currentFlickerInterval = max(0.035f, 0.35f - ((evolutionTimer - 0.70f) / 3.5f) * 0.315f);

        if (flickerTimer >= currentFlickerInterval) {
            flickerTimer = 0.0f;
            showNewStage = !showNewStage;
            silhouetteColorToggle = (silhouetteColorToggle + 1) % 2;

            // Titilar entre Blanco Puro (0xFFFF) y Gris Metálico radiante (0xD69A)
            uint16_t currentShade = (silhouetteColorToggle == 0) ? 0xFFFF : 0xD69A;

            if (oldPetSprite) {
                oldPetSprite->silhouetteColor = currentShade;
                oldPetSprite->visible = !showNewStage; // Muestra la etapa vieja
            }
            if (petSprite) {
                petSprite->silhouetteColor = currentShade;
                petSprite->visible = showNewStage;     // Muestra la etapa nueva
            }
        }
    }
    // FASE 4: DESTELLO BLANCO CEGADOR DE ESTALLIDO (4.2s a 4.45s)
    else if (evolutionTimer >= 4.2f && evolutionTimer < 4.45f) {
        if (evolutionOverlay) {
            evolutionOverlay->setFadeStep(5); // 100% Blanco total
            evolutionOverlay->setDialogueActive(false, false);
        }
        if (oldPetSprite) oldPetSprite->visible = false;
        if (petSprite)    petSprite->visible = false;
    }
    // FASE 5: REVELACIÓN EN FULL COLOR + DIÁLOGO DE VICTORIA (4.45s a 5.6s)
    else if (evolutionTimer >= 4.45f && evolutionTimer < 5.6f) {
        if (evolutionOverlay) {
            evolutionOverlay->setFadeStep(0); // Despejar blanco
            evolutionOverlay->setDialogueActive(true, true); // "¿Felicidades! Ha evolucionado a..."
        }
        
        if (oldPetSprite) {
            removeObject(oldPetSprite);
            oldPetSprite = nullptr;
        }

        if (petSprite) {
            petSprite->silhouetteMode = false; // ¡Desactivar silueta! Revelar en color real
            petSprite->visible = true;
        }
    }
    // FASE 6: FUNDIDO FINAL DE RETORNO AL JUEGO EN 5 PASOS (5.6s a 6.0s)
    else if (evolutionTimer >= 5.6f && evolutionTimer < 6.0f) {
        float progress = (evolutionTimer - 5.6f) / 0.4f;
        uint8_t step = (uint8_t)constrain(progress * 5.0f, 0.0f, 5.0f);
        if (evolutionOverlay) evolutionOverlay->setFadeStep(step);
    }
    // FINALIZACIÓN: Regreso al cuarto normal limpio
    else if (evolutionTimer >= 6.0f) {
        isEvolving = false;

        if (evolutionVortex) {
            removeObject(evolutionVortex);
            evolutionVortex = nullptr;
        }
        if (evolutionOverlay) {
            removeObject(evolutionOverlay);
            evolutionOverlay = nullptr;
        }

        // Restaurar elementos del juego
        if (clockWidget)   clockWidget->visible = true;
        if (statusHUD)     statusHUD->visible = true;
        if (thoughtBubble) thoughtBubble->visible = true;
        if (btnFood)       btnFood->visible = true;
        if (btnMed)        btnMed->visible = true;
        if (btnClean)      btnClean->visible = true;
        if (btnLamp)       btnLamp->visible = true;

        if (petSprite) {
            petSprite->silhouetteMode = false;
            petSprite->visible = true;
        }

        Serial.println("[EVOLUCIÓN GBA] ¡Secuencia completada con éxito!");
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
            petSprite->layer = RenderLayer::Characters; // Capa 2
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
    if (!isEvolving && pet.getStage() != lastKnownStage) {
        PetStage oldStage = lastKnownStage;
        lastKnownStage = pet.getStage();
        startEvolutionSequence(oldStage);
    }

    if (isEvolving) {
        processEvolutionSequence(dt);
        return; 
    }

    if (isMinigameActive) {
        processMinigame(dt);
        return;
    }

    if (petCooldownTimer > 0.0f) petCooldownTimer -= dt;

    if (buttonPressFeedbackTimer > 0.0f) {
        buttonPressFeedbackTimer -= dt;
        if (buttonPressFeedbackTimer <= 0.0f) {
            if (btnFood)  btnFood->position.y = 275;
            if (btnMed)   btnMed->position.y = 275;
            if (btnClean) btnClean->position.y = 275;
            if (btnLamp)  btnLamp->position.y = 275;
            activePressedButton = -1;
        }
    }

    updateSpriteTexture();

    int currentPoops = pet.getPoopCount();
    for (int i = 0; i < 3; i++) {
        if (poopSprites[i]) {
            poopSprites[i]->visible = (i < currentPoops);
        }
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

void PetScene::startMinigame() {
    if (pet.getStage() == PetStage::Egg || pet.getState() == PetState::Sleeping) return;
    isMinigameActive = true;
    minigameRound = 1;
    minigameScore = 0;
    minigameTimer = 0.0f;
    waitingPlayerChoice = true;
    Serial.println("\n[MINIJUEGO] ¡Adivina hacia dónde mirará TAMA! Toca la mitad IZQUIERDA o DERECHA.");
}

void PetScene::processMinigame(float dt) {
    if (!waitingPlayerChoice) {
        minigameTimer += dt;
        if (minigameTimer >= 1.2f) { 
            if (minigameRound < 5) {
                minigameRound++;
                waitingPlayerChoice = true;
                if (petSprite) petSprite->flipX = false;
            } else {
                isMinigameActive = false;
                if (petSprite) petSprite->flipX = false;
                
                if (minigameScore >= 3) {
                    Serial.printf("[MINIJUEGO] ¡VICTORIA! Acertaste %d/5. Felicidad al máximo.\n", minigameScore);
                    pet.pet(); 
                } else {
                    Serial.printf("[MINIJUEGO] Derrota. Solo acertaste %d/5.\n", minigameScore);
                }
            }
        }
    }
}

void PetScene::onTouchReleased() {
    accumulatedStroke = 0.0f;
    prevPetTouchX = 0;
}

void PetScene::onTouch(uint16_t x, uint16_t y) {
    if (isEvolving) return; 

    if (!pet.isLightOn()) {
        if (y >= 250 && x >= 126 && x <= 165 && btnLamp) { 
            pet.toggleLights(); 
            btnLamp->position.y = 278; 
            activePressedButton = 3;
            buttonPressFeedbackTimer = 0.15f;
        }
        return;
    }

    if (isMinigameActive && waitingPlayerChoice) {
        int playerChoice = (x < 86) ? 0 : 1; 
        petChoice = random(0, 2); 
        
        if (petSprite) {
            petSprite->flipX = (petChoice == 1); 
        }

        if (playerChoice == petChoice) {
            minigameScore++;
            Serial.printf("[MINIJUEGO] Ronda %d: ¡ACERTASTE! (Puntaje: %d)\n", minigameRound, minigameScore);
        } else {
            Serial.printf("[MINIJUEGO] Ronda %d: Fallaste. (Puntaje: %d)\n", minigameRound, minigameScore);
        }

        waitingPlayerChoice = false;
        minigameTimer = 0.0f;
        return;
    }

    if (y >= 250) {
        accumulatedStroke = 0.0f;
        prevPetTouchX = 0;

        if (x >= 5 && x <= 45 && btnFood) { 
            pet.feed(); 
            btnFood->position.y = 278; 
            activePressedButton = 0;
            buttonPressFeedbackTimer = 0.15f;
        }
        else if (x >= 46 && x <= 85 && btnMed) { 
            pet.heal(); 
            btnMed->position.y = 278; 
            activePressedButton = 1;
            buttonPressFeedbackTimer = 0.15f;
        }
        else if (x >= 86 && x <= 125 && btnClean) { 
            pet.clean(); 
            btnClean->position.y = 278; 
            activePressedButton = 2;
            buttonPressFeedbackTimer = 0.15f;
        }
        else if (x >= 126 && x <= 165 && btnLamp) { 
            pet.toggleLights(); 
            btnLamp->position.y = 278; 
            activePressedButton = 3;
            buttonPressFeedbackTimer = 0.15f;
        }
        return;
    }

    if (pet.getPoopCount() > 0) {
        bool touchRightPoops = (x >= 115 && x <= 170 && y >= 195 && y <= 250);
        bool touchLeftPoop   = (x >= 0 && x <= 50 && y >= 195 && y <= 245);
        if (touchRightPoops || touchLeftPoop) {
            pet.clean();
            accumulatedStroke = 0.0f;
            prevPetTouchX = 0;
            return;
        }
    }

    if (x >= 45 && x <= 125 && y >= 110 && y <= 195) {
        if (prevPetTouchX > 0) {
            int16_t dx = abs((int16_t)x - (int16_t)prevPetTouchX);
            if (dx > 3 && dx < 30) { 
                accumulatedStroke += dx;
            }
        }
        prevPetTouchX = x;

        if (accumulatedStroke >= 65.0f && petCooldownTimer <= 0.0f) {
            pet.pet();
            accumulatedStroke = 0.0f;
            prevPetTouchX = 0;
            petCooldownTimer = 1.5f;
            Serial.println("[TOUCH] ¡Gesto de caricia horizontal completado!");
        }
    } else {
        accumulatedStroke = 0.0f;
        prevPetTouchX = 0;
    }
}
