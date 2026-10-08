// src/tamagotchi/PetScene.cpp
#include "PetScene.h"
#include "../assets/AssetManager.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <math.h>

PetScene::PetScene(Pet& petRef) : pet(petRef) {}

void PetScene::enter() {
    clockWidget = std::make_shared<ClockWidget>();
    addObject(clockWidget);

    gearWidget = std::make_shared<GearButtonWidget>();
    addObject(gearWidget);

    statusHUD = std::make_shared<StatusHUD>(pet);
    addObject(statusHUD);

    thoughtBubble = std::make_shared<ThoughtBubble>(pet);
    addObject(thoughtBubble);

    messagePopup = std::make_shared<MessagePopup>();
    addObject(messagePopup);
    MessageManager::getInstance().init();

    configHelpModal = std::make_shared<ConfigHelpModal>(pet);
    configHelpModal->setOnEditNameRequested([this]() {
        if (onEditPlayerName) {
            onEditPlayerName();
        }
    });
    addObject(configHelpModal);

    personalityRevealModal = std::make_shared<PersonalityRevealModal>();
    addObject(personalityRevealModal);

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
    dreamTimer = 0.0f;
    nextDreamInterval = (float)random(45, 60);
    updateSpriteTexture();
}

void PetScene::exit() {
    clearObjects();
    petSprite = oldPetSprite = nullptr;
    btnFood = btnMed = btnClean = btnLamp = nullptr;
    for (int i = 0; i < 3; i++) poopSprites[i] = nullptr;
    clockWidget = nullptr;
    gearWidget = nullptr;
    statusHUD = nullptr;
    thoughtBubble = nullptr;
    evolutionVortex = nullptr;
    evolutionOverlay = nullptr;
    messagePopup = nullptr;
    configHelpModal = nullptr;
    personalityRevealModal = nullptr;
}

void PetScene::startAnimationTest() {
    if (isEvolving || isConfigModalActive()) return;
    availableTestAnims.clear();

    static const PetStage stagesToScan[] = {
        PetStage::Egg, PetStage::Baby, PetStage::Child, PetStage::Adult, PetStage::Senior
    };
    static const char* animsToScan[] = {
        "idle", "happy", "eating", "sleeping", "sick", "sad", "dead"
    };

    for (PetStage st : stagesToScan) {
        String stName = stageToString(st);
        for (const char* an : animsToScan) {
            String path = "/tama/sprites/base/" + pet.getSpecies() + "/" + stName + "/" + an + ".png";
            if (SD_MMC.exists(path)) {
                availableTestAnims.push_back({st, String(an), path});
            }
        }
    }

    if (availableTestAnims.empty()) {
        Serial.println("[TEST-ANIMS] No se encontraron animaciones instaladas en la SD.");
        return;
    }

    isAnimTestActive = true;
    animTestIndex = 0;
    animTestTimer = 0.0f;
    Serial.println("\n==================================================");
    Serial.printf("  SHOWCASE DINÁMICO: %d ANIMACIONES ENCONTRADAS\n", (int)availableTestAnims.size());
    Serial.println("==================================================");
    loadTestAnimation();
}

void PetScene::loadTestAnimation() {
    if (animTestIndex >= availableTestAnims.size()) return;

    const auto& entry = availableTestAnims[animTestIndex];
    auto tex = AssetManager::getInstance().getTexture(entry.path.c_str());

    if (tex && petSprite) {
        petSprite->texture = tex;
        petSprite->frameSize = {48, 48};
        petSprite->frameOffset = {0, 0};
        petSprite->position = { (int16_t)((172 - 144) / 2), (int16_t)((320 - 144) / 2) };
        totalFrames = max(1, tex->width / 48);
        currentFrame = 0;

        Serial.printf("[TEST-ANIMS] [%2d/%2d] %-7s / %-8s (%d frames) -> 2.0s\n", 
                      (int)(animTestIndex + 1), (int)availableTestAnims.size(), 
                      stageToString(entry.stage).c_str(), entry.animName.c_str(), totalFrames);
    }
}

void PetScene::processAnimationTest(float dt) {
    animTestTimer += dt;
    if (animTestTimer >= 2.0f) {
        animTestTimer = 0.0f;
        animTestIndex++;
        if (animTestIndex >= availableTestAnims.size()) {
            isAnimTestActive = false;
            Serial.println("[TEST-ANIMS] Showcase finalizado con exito. Volviendo a la mascota.");
            currentTexturePath = ""; 
            updateSpriteTexture();
            return;
        }
        loadTestAnimation();
    }
}

void PetScene::startEvolutionSequence(PetStage oldStage) {
    evolutionOldStage = oldStage;
    isEvolving = true;
    evolutionTimer = 0.0f;
    flickerTimer = 0.0f;
    currentFlickerInterval = 0.35f;
    showNewStage = false;
    silhouetteColorToggle = 0;

    if (configHelpModal) configHelpModal->dismiss();
    if (clockWidget)     clockWidget->visible = false;
    if (gearWidget)      gearWidget->visible = false;
    if (statusHUD)       statusHUD->visible = false;
    if (thoughtBubble)   thoughtBubble->visible = false;
    if (messagePopup)    messagePopup->visible = false;
    if (btnFood)         btnFood->visible = false;
    if (btnMed)          btnMed->visible = false;
    if (btnClean)        btnClean->visible = false;
    if (btnLamp)         btnLamp->visible = false;
    for (int i = 0; i < 3; i++) {
        if (poopSprites[i]) poopSprites[i]->visible = false;
    }

    evolutionVortex = std::make_shared<EvolutionVortex>();
    evolutionVortex->visible = false; 
    addObject(evolutionVortex);

    evolutionOverlay = std::make_shared<EvolutionOverlay>();
    evolutionOverlay->setStageNames(stageToString(oldStage), stageToString(pet.getStage()));
    evolutionOverlay->setFadeStep(0);
    addObject(evolutionOverlay);

    oldPetSprite = petSprite;
    if (oldPetSprite) {
        oldPetSprite->silhouetteMode = false;
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

    Serial.println("\n[EVOLUCIÓN GBA] Iniciando fundido blanco y alternancia de siluetas!");
}

void PetScene::processEvolutionSequence(float dt) {
    evolutionTimer += dt;

    if (evolutionVortex)  evolutionVortex->update(dt);
    if (evolutionOverlay) evolutionOverlay->update(dt);

    if (evolutionTimer < 0.35f) {
        uint8_t step = (uint8_t)constrain((evolutionTimer / 0.35f) * 5.0f, 0.0f, 5.0f);
        if (evolutionOverlay) {
            evolutionOverlay->setFadeStep(step);
            evolutionOverlay->setDialogueActive(false, false);
        }
    }
    else if (evolutionTimer >= 0.35f && evolutionTimer < 0.70f) {
        if (evolutionVortex) evolutionVortex->visible = true;
        if (oldPetSprite) {
            oldPetSprite->silhouetteMode = true;
            oldPetSprite->silhouetteColor = 0xFFFF;
        }

        float progress = (evolutionTimer - 0.35f) / 0.35f;
        uint8_t step = 5 - (uint8_t)constrain(progress * 5.0f, 0.0f, 5.0f);
        if (evolutionOverlay) {
            evolutionOverlay->setFadeStep(step);
            evolutionOverlay->setDialogueActive(true, false);
        }
    }
    else if (evolutionTimer >= 0.70f && evolutionTimer < 4.2f) {
        if (evolutionOverlay) evolutionOverlay->setFadeStep(0);

        flickerTimer += dt;
        currentFlickerInterval = max(0.035f, 0.35f - ((evolutionTimer - 0.70f) / 3.5f) * 0.315f);

        if (flickerTimer >= currentFlickerInterval) {
            flickerTimer = 0.0f;
            showNewStage = !showNewStage;
            silhouetteColorToggle = (silhouetteColorToggle + 1) % 2;

            uint16_t currentShade = (silhouetteColorToggle == 0) ? 0xFFFF : 0xD69A;

            if (oldPetSprite) {
                oldPetSprite->silhouetteColor = currentShade;
                oldPetSprite->visible = !showNewStage;
            }
            if (petSprite) {
                petSprite->silhouetteColor = currentShade;
                petSprite->visible = showNewStage;
            }
        }
    }
    else if (evolutionTimer >= 4.2f && evolutionTimer < 4.45f) {
        if (evolutionOverlay) {
            evolutionOverlay->setFadeStep(5);
            evolutionOverlay->setDialogueActive(false, false);
        }
        if (oldPetSprite) oldPetSprite->visible = false;
        if (petSprite)    petSprite->visible = false;
    }
    else if (evolutionTimer >= 4.45f && evolutionTimer < 5.6f) {
        if (evolutionOverlay) {
            evolutionOverlay->setFadeStep(0);
            evolutionOverlay->setDialogueActive(true, true);
        }
        
        if (oldPetSprite) {
            removeObject(oldPetSprite);
            oldPetSprite = nullptr;
        }

        if (petSprite) {
            petSprite->silhouetteMode = false;
            petSprite->visible = true;
        }
    }
    else if (evolutionTimer >= 5.6f && evolutionTimer < 6.0f) {
        float progress = (evolutionTimer - 5.6f) / 0.4f;
        uint8_t step = (uint8_t)constrain(progress * 5.0f, 0.0f, 5.0f);
        if (evolutionOverlay) evolutionOverlay->setFadeStep(step);
    }
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

        if (clockWidget)   clockWidget->visible = true;
        if (gearWidget)    gearWidget->visible = true;
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

        Serial.println("[EVOLUCIÓN GBA] Secuencia completada con exito!");

        if (evolutionOldStage == PetStage::Egg && personalityRevealModal) {
            personalityRevealModal->showReveal(pet.getGenome());
        }
    }
}

void PetScene::updateSpriteTexture() {
    String newPath = pet.getSpritePath();
    if (newPath == currentTexturePath && petSprite != nullptr) return;

    currentTexturePath = newPath;
    // Inyectar la semilla única de esta mascota al gestor de Texturas
    AssetManager::getInstance().setActiveDnaSeed(pet.getDnaSeed());
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
    if (isAnimTestActive) {
        processAnimationTest(dt);
        if (petSprite && totalFrames > 1) {
            animTimer += dt;
            if (animTimer >= 0.25f) {
                animTimer = 0.0f;
                currentFrame = (currentFrame + 1) % totalFrames;
                petSprite->frameOffset.x = currentFrame * 48;
            }
        }
        return;
    }

    if (!isEvolving && pet.getState() != PetState::Dead && pet.getStage() != PetStage::Dead && pet.getStage() != lastKnownStage) {
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

    telemetryBroadcastTimer += dt;
    if (telemetryBroadcastTimer >= 0.1f) {
        telemetryBroadcastTimer = 0.0f;
        if (pet.getBrain()) {
            Serial.println("!BRAIN:" + pet.getBrain()->getTelemetryJson());
            Serial.printf("!GENOME:{\"tag\":\"%s\",\"metab\":%.2f,\"soc\":%.2f,\"res\":%.2f,\"slp\":%.2f}\n",
                          pet.getGenome().personalityTag.c_str(),
                          pet.getGenome().metabolismRate,
                          pet.getGenome().socialNeed,
                          pet.getGenome().resilience,
                          pet.getGenome().sleepPacing);
        }
    }

    // Consumo de mensajes con ventana de lectura ampliada a 6.0s
    if (MessageManager::getInstance().hasPendingMessage()) {
        if (messagePopup) {
            bool isDream = MessageManager::getInstance().isPendingDream();
            messagePopup->showMessage(MessageManager::getInstance().consumeMessage(), 6.0f, isDream);
        }
    }

    if (pet.getStage() != PetStage::Egg && pet.getState() != PetState::Dead) {
        if (pet.getState() == PetState::Sleeping) {
            spontaneousThoughtTimer = 0.0f;

            dreamTimer += dt;
            if (dreamTimer >= nextDreamInterval) {
                dreamTimer = 0.0f;
                nextDreamInterval = (float)random(45, 60);

                bool isRestless = (pet.getHunger() < 30.0f || pet.getHealth() < 60.0f || pet.isLightOn() || pet.getPoopCount() > 0);
                float affWeight = pet.getBrain() ? pet.getBrain()->getAffectionWeight() : 0.8f;
                bool hasAffection = (affWeight > 1.0f);

                MessageManager::getInstance().requestDreamMessage(pet.getStage(), isRestless, hasAffection);
            }
        } else {
            dreamTimer = 0.0f;

            spontaneousThoughtTimer += dt;
            if (spontaneousThoughtTimer >= 40.0f) {
                spontaneousThoughtTimer = 0.0f;

                if (pet.getState() == PetState::Sick || pet.getHealth() < 40.0f) {
                    MessageManager::getInstance().requestAiMessage("sick", 
                        "Salud critica (" + String((int)pet.getHealth()) + "%). Necesita medicina urgente.",
                        pet.getGenome().personalityTag);
                }
                else if (pet.getHunger() < 30.0f) {
                    MessageManager::getInstance().requestAiMessage("eating", 
                        "Hambre critica (" + String((int)pet.getHunger()) + "%). Deseo de comida.",
                        pet.getGenome().personalityTag);
                }
                else if (pet.getHappiness() > 70.0f && pet.getHealth() >= 60.0f) {
                    MessageManager::getInstance().requestAiMessage("happy", 
                        "Felicidad (" + String((int)pet.getHappiness()) + "% > 70%) y salud consolidada",
                        pet.getGenome().personalityTag);
                }
            }
        }
    }

    if (messagePopup) messagePopup->update(dt);
    if (configHelpModal) configHelpModal->update(dt);
    if (personalityRevealModal) personalityRevealModal->update(dt); 

    updateSpriteTexture();

    if (petSprite) {
        petSprite->position.y = (int16_t)((320 - 144) / 2);

        if (totalFrames > 1) {
            animTimer += dt;
            if (animTimer >= 0.25f) {
                animTimer = 0.0f;
                currentFrame = (currentFrame + 1) % totalFrames;
                petSprite->frameOffset.x = currentFrame * 48;
            }
        } else {
            petSprite->frameOffset.x = 0;
        }
    }

    int currentPoops = pet.getPoopCount();
    for (int i = 0; i < 3; i++) {
        if (poopSprites[i]) {
            poopSprites[i]->visible = (i < currentPoops);
        }
    }

    if (btnLamp) {
        btnLamp->frameOffset.x = pet.isLightOn() ? 16 : 32;
    }
}

void PetScene::startMinigame() {
    if (isConfigModalActive() || pet.getStage() == PetStage::Egg || pet.getState() == PetState::Sleeping || pet.getState() == PetState::Dead) return;
    isMinigameActive = true;
    minigameRound = 1;
    minigameScore = 0;
    minigameTimer = 0.0f;
    waitingPlayerChoice = true;
    Serial.println("\n[MINIJUEGO] Adivina hacia donde mirara TAMA! Toca la mitad IZQUIERDA o DERECHA.");
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
                    Serial.printf("[MINIJUEGO] VICTORIA! Acertaste %d/5. Felicidad al maximo.\n", minigameScore);
                    pet.pet(); 
                } else {
                    Serial.printf("[MINIJUEGO] Derrota. Solo acertaste %d/5.\n", minigameScore);
                }
            }
        }
    }
}

void PetScene::onTouchReleased() {
    if (isConfigModalActive()) {
        configHelpModal->onTouchReleased();
        return;
    }
    accumulatedStroke = 0.0f;
    prevPetTouchX = 0;
}

void PetScene::onTouch(uint16_t x, uint16_t y) {
    // Prioridad absoluta al modal de revelación de personalidad
    if (personalityRevealModal && personalityRevealModal->isVisible()) {
        if (personalityRevealModal->getClick(x, y) == 1) {
            personalityRevealModal->dismiss();
        }
        return; 
    }

    // 1. Modal activo tiene prioridad absoluta
    if (isConfigModalActive()) {
        configHelpModal->onTouch(x, y);
        return;
    }

    // 2. Descartar popups activos
    if (messagePopup && messagePopup->visible) {
        messagePopup->dismiss();
        return;
    }

    // 3. Toque en el botón engranaje de configuración: Hitbox x: [126, 168], y: [26, 54]
    if (x >= 126 && x <= 168 && y >= 26 && y <= 54) {
        if (configHelpModal) {
            configHelpModal->show("GUIA & AJUSTES");
            accumulatedStroke = 0.0f;
            prevPetTouchX = 0;
            return;
        }
    }

    // 4. Mascota fallecida
    if (pet.getState() == PetState::Dead || pet.getStage() == PetStage::Dead) {
        deathTouchTriggered = true;
        return;
    }

    // 5. Cancelar showcase si está corriendo
    if (isAnimTestActive) {
        isAnimTestActive = false;
        Serial.println("[TEST-ANIMS] Showcase cancelado por toque en pantalla.");
        currentTexturePath = "";
        updateSpriteTexture();
        return;
    }

    if (isEvolving) return; 

    // 6. Lámpara apagada: solo permite encender luz
    if (!pet.isLightOn()) {
        if (y >= 250 && x >= 126 && x <= 165 && btnLamp) { 
            pet.toggleLights(); 
            btnLamp->position.y = 278; 
            activePressedButton = 3;
            buttonPressFeedbackTimer = 0.15f;
        }
        return;
    }

    // 7. Entrada de minijuego
    if (isMinigameActive && waitingPlayerChoice) {
        int playerChoice = (x < 86) ? 0 : 1; 
        petChoice = random(0, 2); 
        
        if (petSprite) {
            petSprite->flipX = (petChoice == 1); 
        }

        if (playerChoice == petChoice) {
            minigameScore++;
            Serial.printf("[MINIJUEGO] Ronda %d: ACERTASTE! (Puntaje: %d)\n", minigameRound, minigameScore);
        } else {
            Serial.printf("[MINIJUEGO] Ronda %d: Fallaste. (Puntaje: %d)\n", minigameRound, minigameScore);
        }

        waitingPlayerChoice = false;
        minigameTimer = 0.0f;
        return;
    }

    // 8. Botones inferiores
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

    // 9. Cacas en el suelo
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

    // 10. Caricias sobre la mascota
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
            Serial.println("[TOUCH] Gesto de caricia completado!");
        }
    } else {
        accumulatedStroke = 0.0f;
        prevPetTouchX = 0;
    }
}
