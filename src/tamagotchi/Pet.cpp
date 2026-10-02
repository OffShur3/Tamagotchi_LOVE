// src/tamagotchi/Pet.cpp
#include "Pet.h"
#include <SD_MMC.h>
#include <ArduinoJson.h>

struct SpiRamAllocator {
    void* allocate(size_t size) { void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); return p ? p : malloc(size); }
    void deallocate(void* pointer) { free(pointer); }
    void* reallocate(void* ptr, size_t new_size) { void* p = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); return p ? p : realloc(ptr, new_size); }
};
typedef BasicJsonDocument<SpiRamAllocator> SpiRamJsonDocument;

static float physicalTouchTimer = 0.0f;

Pet::Pet() {
    brain = std::unique_ptr<IBrain>(new Brain());
}

void Pet::setBrain(std::unique_ptr<IBrain> newBrain) {
    if (newBrain) {
        brain = std::move(newBrain);
        brain->init();
        Serial.printf("[PET] ¡Transplante de cerebro exitoso! Nuevo cerebro: %s\n", brain->getName());
    }
}

void Pet::init() {
    if (!load()) {
        Serial.println("[PET] No se encontró partida. Inicializando huevo...");
        reset();
    }
    if (brain) brain->init();
    catchUpTime();
}

void Pet::catchUpTime() {
    time_t now = time(NULL);
    if (now < 1700000000) return; 

    if (stage == PetStage::Dead || state == PetState::Dead) return;

    if (lastTimestamp > 0 && lastTimestamp >= 1700000000 && now > lastTimestamp) {
        uint32_t elapsed = (uint32_t)(now - lastTimestamp);

        if (elapsed > 2 && elapsed <= 10800) {
            Serial.printf("[PET] Procesando %u segundos transcurridos fuera de línea...\n", elapsed);
            uint32_t remaining = elapsed;
            while (remaining > 0) {
                float step = (remaining > 60) ? 60.0f : (float)remaining;
                update(step);
                remaining -= (uint32_t)step;
            }
            save(); 
        } else if (elapsed > 10800) {
            Serial.printf("[PET] Gran salto temporal detectado (%u s). Sincronizando hora sin penalización.\n", elapsed);
            save();
        }
    }
    lastTimestamp = (uint32_t)now;
    printStats();
}

void Pet::printStats() const {
    float lifePercent = (age / TOTAL_LIFESPAN_SECONDS) * 100.0f;
    
    Serial.println("\n=================================");
    Serial.println("       TAMA PET DIAGNOSTICS      ");
    Serial.println("=================================");
    Serial.printf("  Especie:     %s\n", species.c_str());
    Serial.printf("  Etapa:       %s\n", stageToString(stage).c_str());
    Serial.printf("  Estado:      %s\n", stateToString(state).c_str());
    Serial.printf("  Hambre:      %.1f / 100\n", hunger);
    Serial.printf("  Felicidad:   %.1f / 100\n", happiness);
    Serial.printf("  Energía:     %.1f / 100\n", energy);
    Serial.printf("  Salud:       %.1f / 100\n", health);
    Serial.printf("  Cacas:       %d / 3\n", poopCount);
    Serial.printf("  Luz:         %s\n", lightsOn ? "ENCENDIDA" : "APAGADA");
    Serial.printf("  Edad:        %u segs (%.1f%% de vida)\n", (uint32_t)age, lifePercent);
    Serial.println("---------------------------------");
    Serial.printf("  CEREBRO:     %s\n", brain ? brain->getName() : "Sin Cerebro");
    Serial.printf("  Pensamiento: %s\n", getThoughtIcon() >= 0 ? "Activo en Pantalla" : "En reposo");
    Serial.printf("  Spike Rate:  %.2f spikes/sec (Hz)\n", brain ? brain->getSpikeRate() : 0.0f);
    Serial.printf("  Dopamina:    %.2f / 2.00\n", brain ? brain->getDopamineLevel() : 0.0f);
    Serial.printf("  Stress/Pain: %.2f\n", brain ? brain->getStressLevel() : 0.0f);
    
    if (brain) {
        Brain* b = static_cast<Brain*>(brain.get());
        Serial.println("---------------------------------");
        Serial.println("      IMPULSOS COGNITIVOS        ");
        Serial.println("---------------------------------");
        Serial.printf("  Urgencia Hambre:  %.1f %%\n", b->getHungerDrive());
        Serial.printf("  Urgencia Social:  %.1f %%\n", b->getSocialDrive());
        Serial.printf("  Urgencia Fatiga:  %.1f %%\n", b->getSleepDrive());
        Serial.printf("  Nivel Malestar:   %.1f %%\n", b->getDistressDrive());
    }
    Serial.println("=================================\n");
}

void Pet::update(float dt) {
    if (stage == PetStage::Dead || state == PetState::Dead) {
        stage = PetStage::Dead;
        state = PetState::Dead;
        return;
    }

    age += dt;

    if (dt < 2.0f) { 
        autoSaveTimer += dt;
        if (autoSaveTimer >= 30.0f) {
            autoSaveTimer = 0.0f;
            save();
            Serial.println("[PET] Auto-guardado de rutina en SD (30s).");
        }
    }

    if (actionTimer > 0.0f) {
        actionTimer -= dt;
        if (actionTimer <= 0.0f && state != PetState::Dead) {
            state = (happiness < 25.0f || hunger < 25.0f) ? PetState::Sad : PetState::Idle;
        }
    }

    if (physicalTouchTimer > 0.0f) {
        physicalTouchTimer -= dt;
    }

    checkEvolution();

    if (stage != PetStage::Egg) {
        float stageHungerMult = (stage == PetStage::Baby) ? 1.5f : 1.0f;
        float stageEnergyMult = (stage == PetStage::Baby || stage == PetStage::Senior) ? 1.4f : 1.0f;

        float hungerRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.06f)) * stageHungerMult;
        hunger = max(0.0f, hunger - (hungerRate * dt));

        float happinessRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.10f));
        happiness = max(0.0f, happiness - (happinessRate * dt));

        if (!lightsOn) {
            if (state == PetState::Sleeping) {
                float energyGain = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.02f));
                energy = min(100.0f, energy + (energyGain * dt));
                if (energy >= 100.0f) {
                    energy = 100.0f;
                    state = PetState::Idle;
                    Serial.println("[PET] Mascota recuperó el 100% de energía. Se despertó sola.");
                }
            } else {
                if (energy >= 100.0f) {
                    energy = 100.0f;
                } else {
                    float energyRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.20f));
                    energy = max(0.0f, energy - (energyRate * dt));
                }
            }
        } else {
            float energyRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.12f)) * stageEnergyMult;
            energy = max(0.0f, energy - (energyRate * dt));
        }

        float poopInterval = TOTAL_LIFESPAN_SECONDS * 0.025f;
        if (hunger > 40.0f && poopCount < 3) {
            poopTimer += dt;
            if (poopTimer >= poopInterval) {
                poopCount++;
                poopTimer = 0.0f;
                Serial.printf("[PET] ¡La mascota hizo caca! Total: %d\n", poopCount);
            }
        }

        if (poopCount > 0 || hunger <= 0.0f || happiness <= 0.0f) {
            float healthDropRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.02f)) * (poopCount + 1);
            health = max(0.0f, health - (healthDropRate * dt));
        }
    }

    if (brain) {
        PetSensoryInput input = {
            hunger, energy, happiness, health, poopCount, lightsOn,
            (physicalTouchTimer > 0.0f)
        };
        brain->update(dt, input);
    }

    checkStateTransitions();
}

void Pet::checkEvolution() {
    if (stage == PetStage::Dead || state == PetState::Dead) return;

    float tEgg    = TOTAL_LIFESPAN_SECONDS * EGG_RATIO;
    float tBaby   = tEgg  + (TOTAL_LIFESPAN_SECONDS * BABY_RATIO);
    float tChild  = tBaby + (TOTAL_LIFESPAN_SECONDS * CHILD_RATIO);
    float tSenior = TOTAL_LIFESPAN_SECONDS; 

    PetStage newStage = stage;

    if (age < tEgg)            { newStage = PetStage::Egg; }
    else if (age < tBaby)      { newStage = PetStage::Baby; }
    else if (age < tChild)     { newStage = PetStage::Child; }
    else if (age < tSenior)    { newStage = PetStage::Adult; }
    else if (age >= tSenior)   { newStage = PetStage::Senior; }

    if (age >= TOTAL_LIFESPAN_SECONDS + (TOTAL_LIFESPAN_SECONDS * SENIOR_RATIO)) {
        stage = PetStage::Dead;
        state = PetState::Dead;
        Serial.println("[PET] La mascota ha fallecido de forma natural (Vejez).");
        save();
        return;
    }

    if (newStage != stage) {
        if (stage == PetStage::Egg && newStage == PetStage::Baby) {
            auto available = discoverInstalledSpecies();
            species = available[random(0, available.size())];
            Serial.printf("[PET] ¡Eclosión! Raza asignada: %s\n", species.c_str());
        }

        stage = newStage;
        Serial.printf("[PET] ¡Evolución! Nueva etapa: %s\n", stageToString(stage).c_str());
        save();
    }
}

void Pet::checkStateTransitions() {
    if (stage == PetStage::Dead || state == PetState::Dead || health <= 0.0f) { 
        stage = PetStage::Dead; 
        state = PetState::Dead; 
        return; 
    }
    
    // Si hay una acción de interacción física deliberada en curso (comer durante 2.5s), no sobreescribir
    if (actionTimer > 0.0f) return;

    if (brain) {
        state = brain->getDecision();
    }
}

// SOLUCIÓN: Único disparador de Eating en el sistema
void Pet::feed() {
    if (stage == PetStage::Egg || stage == PetStage::Dead || state == PetState::Dead) return;
    hunger = min(100.0f, hunger + 35.0f);
    state = PetState::Eating;
    actionTimer = 2.5f; // Come activamente durante 2.5 segundos
    
    if (brain) {
        brain->emitReward(0.3f);
        brain->onFed(); // Reinicia el sensor neuronal de hambre
    }
    Serial.println("[PET] Alimentando a la mascota. Estado Eating activo (2.5s).");
    save();
}

void Pet::pet() {
    if (stage == PetStage::Dead || state == PetState::Dead) return;
    if (stage == PetStage::Egg) { 
        age += (TOTAL_LIFESPAN_SECONDS * 0.005f); 
        Serial.println("[PET] ¡Tocaste el huevo! Acelerando eclosión...");
        return; 
    }
    happiness = min(100.0f, happiness + 25.0f);
    
    if (happiness >= 35.0f) {
        state = PetState::Happy;
        actionTimer = 2.5f;
    } else {
        Serial.println("[PET] Acariciaste a la mascota, pero sigue triste.");
    }

    physicalTouchTimer = 0.3f;
    if (brain) brain->emitReward(0.6f);
    save();
}

void Pet::heal() {
    if (stage == PetStage::Egg || stage == PetStage::Dead || state == PetState::Dead) return;
    health = min(100.0f, health + 60.0f);
    state = PetState::Happy;
    actionTimer = 2.0f;
    if (brain) brain->emitReward(0.5f);
    save();
}

void Pet::clean() {
    if (stage == PetStage::Dead || state == PetState::Dead) return;
    if (poopCount > 0) {
        poopCount = 0;
        poopTimer = 0.0f;
        happiness = min(100.0f, happiness + 15.0f);
        Serial.println("[PET] ¡Suelo limpiado!");
        save();
    }
}

void Pet::toggleLights() {
    if (stage == PetStage::Dead || state == PetState::Dead) return;
    bool wasSleeping = (state == PetState::Sleeping);
    lightsOn = !lightsOn;
    Serial.printf("[PET] Luz %s\n", lightsOn ? "Encendida" : "Apagada");

    if (lightsOn && wasSleeping) {
        if (energy < 75.0f) {
            state = PetState::Sad;
            actionTimer = 3.0f;
            happiness = max(0.0f, happiness - 20.0f);
            Serial.printf("[PET] ¡Luz encendida bruscamente (Energía: %.1f%% < 75%%)! Mascota despertada con estrés.\n", energy);
            if (brain) {
                brain->emitReward(-0.8f);
            }
        } else {
            state = PetState::Idle;
            Serial.printf("[PET] La mascota descansó bien (Energía: %.1f%% >= 75%%). Despertó con calma.\n", energy);
        }
    }
    save();
}

void Pet::forcePoop() {
    if (stage == PetStage::Dead || state == PetState::Dead) return;
    poopCount = min(3, poopCount + 1);
    Serial.printf("[TEST] Caca forzada. Total en suelo: %d\n", poopCount);
    save();
}

void Pet::drainEnergy(float amount) {
    if (stage == PetStage::Dead || state == PetState::Dead) return;
    energy = max(0.0f, energy - amount);
    Serial.printf("[TEST] Energía drenada a: %.1f\n", energy);
    save();
}

void Pet::makeSick() {
    if (stage == PetStage::Dead || state == PetState::Dead) return;
    health = max(0.0f, health - 50.0f);
    Serial.printf("[TEST] Salud reducida a: %.1f\n", health);
    save();
}

void Pet::accelerateAge(float seconds) {
    if (stage == PetStage::Dead || state == PetState::Dead) return;
    age += seconds;
    Serial.printf("[TEST] Tiempo acelerado en +%.0f s. Nueva edad: %.0f s\n", seconds, age);
    checkEvolution();
    save();
}

String Pet::getSpritePath() const {
    String folder = stageToString(stage);
    String anim   = stateToString(state);

    String target = "/tama/sprites/base/" + species + "/" + folder + "/" + anim + ".png";
    if (SD_MMC.exists(target)) return target;

    String fallbackIdle = "/tama/sprites/base/" + species + "/" + folder + "/idle.png";
    if (SD_MMC.exists(fallbackIdle)) return fallbackIdle;

    String tiernitoTarget = "/tama/sprites/base/tiernito/" + folder + "/" + anim + ".png";
    if (SD_MMC.exists(tiernitoTarget)) return tiernitoTarget;

    String tiernitoIdle = "/tama/sprites/base/tiernito/" + folder + "/idle.png";
    if (SD_MMC.exists(tiernitoIdle)) return tiernitoIdle;

    return "/tama/sprites/base/tiernito/huevo/idle.png";
}

std::vector<String> Pet::discoverInstalledSpecies() {
    std::vector<String> list;
    File root = SD_MMC.open("/tama/sprites/base");
    if (!root || !root.isDirectory()) { list.push_back("tiernito"); return list; }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            String name = String(file.name());
            int lastSlash = name.lastIndexOf('/');
            if (lastSlash != -1) name = name.substring(lastSlash + 1);
            if (name.length() > 0) list.push_back(name);
        }
        file = root.openNextFile();
    }
    if (list.empty()) list.push_back("tiernito");
    return list;
}

bool Pet::load() {
    if (!SD_MMC.exists("/config/save.json")) return false;
    File f = SD_MMC.open("/config/save.json", "r");
    if (!f) return false;

    SpiRamJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    species       = doc["species"] | "tiernito";
    stage         = static_cast<PetStage>(doc["stage"] | 0);
    state         = static_cast<PetState>(doc["state"] | 0);
    hunger        = doc["hunger"] | 100.0f;
    happiness     = doc["happiness"] | 100.0f;
    energy        = doc["energy"] | 100.0f;
    health        = doc["health"] | 100.0f;
    poopCount     = doc["poop"] | 0;
    lightsOn      = doc["lights"] | true;
    age           = doc["age"] | 0.0f;
    lastTimestamp = doc["lastTimestamp"] | 0;

    return true;
}

bool Pet::save() {
    SpiRamJsonDocument doc(2048);
    doc["species"]       = species;
    doc["stage"]         = static_cast<int>(stage);
    doc["state"]         = static_cast<int>(state);
    doc["hunger"]        = hunger;
    doc["happiness"]     = happiness;
    doc["energy"]        = energy;
    doc["health"]        = health;
    doc["poop"]          = poopCount;
    doc["lights"]        = lightsOn;
    doc["age"]           = age;
    doc["lastTimestamp"] = (uint32_t)time(NULL);

    SD_MMC.mkdir("/config");
    File f = SD_MMC.open("/config/save.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

void Pet::reset() {
    species = "tiernito";
    stage = PetStage::Egg;
    state = PetState::Idle;
    hunger = 100.0f; happiness = 100.0f; energy = 100.0f; health = 100.0f;
    poopCount = 0; lightsOn = true; age = 0.0f; actionTimer = 0.0f; poopTimer = 0.0f;
    lastTimestamp = (uint32_t)time(NULL);
    save();
}
