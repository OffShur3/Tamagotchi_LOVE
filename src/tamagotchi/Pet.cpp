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
        Serial.printf("[PET] Transplante de cerebro exitoso! Nuevo cerebro: %s\n", brain->getName());
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
    Serial.printf("  DNA Seed:    0x%08X\n", dnaSeed);
    Serial.printf("  Etapa:       %s\n", stageToString(stage).c_str());
    Serial.printf("  Estado:      %s\n", stateToString(state).c_str());
    Serial.printf("  Hambre:      %.1f / 100\n", hunger);
    Serial.printf("  Felicidad:   %.1f / 100\n", happiness);
    Serial.printf("  Energía:     %.1f / 100\n", energy);
    Serial.printf("  Salud:       %.1f / 100\n", health);
    Serial.printf("  Apego/Trust: %.1f / 100\n", trust);
    Serial.printf("  Cacas:       %d / 3\n", poopCount);
    Serial.printf("  Digestión:   %s (Timer: %.0fs)\n", digesting ? "EN CURSO" : "INACTIVA", digestiveTransitTimer);
    Serial.printf("  Luz:         %s\n", lightsOn ? "ENCENDIDA" : "APAGADA");
    Serial.printf("  Edad:        %u segs (%.1f%% de vida)\n", (uint32_t)age, lifePercent);
    Serial.println("---------------------------------");
    Serial.printf("  CEREBRO:     %s\n", brain ? brain->getName() : "Sin Cerebro");
    Serial.printf("  Spike Rate:  %.2f spikes/sec (Hz)\n", brain ? brain->getSpikeRate() : 0.0f);
    Serial.printf("  Dopamina:    %.2f / 2.00\n", brain ? brain->getDopamineLevel() : 0.0f);
    Serial.printf("  Stress/Pain: %.2f\n", brain ? brain->getStressLevel() : 0.0f);
    Serial.printf("  Rumiación:   %.2f / 2.50\n", brain ? brain->getRuminationLevel() : 0.0f);
    Serial.printf("  Mult Hambre: x%.2f\n", brain ? brain->getHungerMetabolicRate() : 1.0f);
    Serial.printf("  Mult Energía:x%.2f\n", brain ? brain->getEnergyDrainMultiplier() : 1.0f);
    Serial.println("=================================\n");
}

void Pet::update(float dt) {
    if (state == PetState::Dead || health <= 0.0f) {
        state = PetState::Dead;
        health = 0.0f;
        
        if (stage == PetStage::Egg) {
            stage = PetStage::Baby;
        }
        
        if (brain) {
            PetSensoryInput deadInput = { 
                0.0f, 0.0f, 0.0f, 0.0f, poopCount, lightsOn, false, stage, false, trust 
            };
            brain->update(dt, deadInput);
        }
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
            state = (happiness < 20.0f || hunger < 20.0f) ? PetState::Sad : PetState::Idle;
        }
    }

    if (physicalTouchTimer > 0.0f) {
        physicalTouchTimer -= dt;
    }

    checkEvolution();

    if (stage != PetStage::Egg) {
        float stageHungerMult = (stage == PetStage::Baby) ? 1.5f : 1.0f;
        float stageEnergyMult = (stage == PetStage::Baby || stage == PetStage::Senior) ? 1.4f : 1.0f;

        float hungerCognitiveMult = brain ? brain->getHungerMetabolicRate() : 1.0f;
        float energyCognitiveMult = brain ? brain->getEnergyDrainMultiplier() : 1.0f;
        float stressCognitiveMult = brain ? brain->getStressMultiplier() : 1.0f;

        float hungerRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.06f)) * stageHungerMult * hungerCognitiveMult;
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
                    Serial.println("[PET] Mascota recupero el 100% de energia. Se desperto sola.");
                }
            } else {
                if (energy >= 100.0f) {
                    energy = 100.0f;
                } else {
                    float energyRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.20f)) * energyCognitiveMult;
                    energy = max(0.0f, energy - (energyRate * dt));
                }
            }
        } else {
            float energyRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.12f)) * stageEnergyMult * energyCognitiveMult;
            energy = max(0.0f, energy - (energyRate * dt));
        }

        // ================= MOTOR DIGESTIVO BIOLÓGICO =================
        if (digesting) {
            if (hunger < 20.0f) {
                digesting = false; // El tránsito digestivo cesa con estómago vacío
            } else {
                digestiveTransitTimer -= dt;
                if (digestiveTransitTimer <= 0.0f) {
                    if (poopCount < 3) {
                        poopCount++;
                        poopExposureTimer = 0.0f;
                        Serial.printf("[PET] Proceso digestivo completado. La mascota hizo caca! (Total: %d/3)\n", poopCount);
                    }
                    digesting = false;
                }
            }
        }

        // ================= PENALIZACIÓN POR NEGLIGENCIA HIGIÉNICA =================
        if (poopCount > 0) {
            poopExposureTimer += dt;
            // Más de 15 minutos sin limpiar: estimulación de dolor físico
            if (poopExposureTimer > 900.0f && brain) {
                static_cast<Brain*>(brain.get())->stimulateNeuron(3, 0.08f * dt);
            }
            // Más de 20 minutos: erosión de confianza y apego
            if (poopExposureTimer > 1200.0f) {
                trust = max(0.0f, trust - (0.2f * dt / 60.0f));
            }
        }

        // Erosión por abandono en hambre crítica o enfermedad prolongada
        if (hunger < 15.0f || (health < 30.0f && state == PetState::Sick)) {
            trust = max(0.0f, trust - (0.3f * dt / 60.0f));
        }

        if (poopCount > 0 || hunger <= 0.0f || happiness <= 0.0f) {
            float healthDropRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.02f)) * (poopCount + 1) * stressCognitiveMult;
            health = max(0.0f, health - (healthDropRate * dt));
        }
    }

    if (brain) {
        PetSensoryInput input = {
            hunger, energy, happiness, health, poopCount, lightsOn,
            (physicalTouchTimer > 0.0f),
            stage,
            (stage == PetStage::Egg),
            trust
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
    if (health <= 0.0f) { 
        if (state != PetState::Dead) {
            state = PetState::Dead;
            Serial.println("\n**************************************************");
            Serial.println("[PET-ALERT] ¡LA MASCOTA HA FALLECIDO POR ENFERMEDAD/DOLOR!");
            Serial.printf("[PET-ALERT] Etapa conservada: %s. Mostrando %s/dead.png\n", stageToString(stage).c_str(), stageToString(stage).c_str());
            Serial.println("**************************************************\n");
            save();
        }
        return; 
    }
    
    if (actionTimer > 0.0f) return;

    if (brain) {
        state = brain->getDecision();
    }
}

void Pet::feed() {
    if (stage == PetStage::Egg || stage == PetStage::Dead || state == PetState::Dead) return;

    // Alimentar cuando tenía hambre (< 40%) construye apego
    if (hunger < 40.0f) {
        trust = min(100.0f, trust + 3.0f);
    }

    hunger = min(100.0f, hunger + 35.0f);
    state = PetState::Eating;
    actionTimer = 2.5f;

    // Activación y aceleración del tránsito digestivo
    float metabolicRate = brain ? brain->getHungerMetabolicRate() : 1.0f;
    float baseTransit = (float)random(900, 1500) / metabolicRate;

    if (digesting) {
        digestiveTransitTimer *= 0.7f; // Aceleración del 30% por sobrealimentación
    } else {
        digestiveTransitTimer = baseTransit;
        digesting = true;
    }

    if (brain) {
        brain->emitReward(0.3f);
        brain->onFed();
    }
    Serial.printf("[PET] Alimentando a la mascota. Transito digestivo: %.0fs. Apego: %.1f%%\n", digestiveTransitTimer, trust);
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
    trust = min(100.0f, trust + 0.5f); // Micro-ganancia constante de confianza
    
    if (happiness >= 35.0f) {
        state = PetState::Happy;
        actionTimer = 2.5f;
    }

    physicalTouchTimer = 0.3f;
    if (brain) brain->emitReward(0.6f);
    save();
}

void Pet::heal() {
    if (stage == PetStage::Egg || stage == PetStage::Dead || state == PetState::Dead) return;
    health = min(100.0f, health + 60.0f);
    trust = min(100.0f, trust + 4.0f);
    state = PetState::Happy;
    actionTimer = 2.0f;
    if (brain) brain->emitReward(0.5f);
    save();
}

void Pet::clean() {
    if (stage == PetStage::Dead || state == PetState::Dead) return;
    if (poopCount > 0) {
        // Limpieza puntual (< 5 minutos) otorga bono de apego
        if (poopExposureTimer < 300.0f) {
            trust = min(100.0f, trust + 4.0f);
            Serial.println("[PET] ¡Limpieza puntual! Bono de apego +4.0%.");
        }
        poopCount = 0;
        poopExposureTimer = 0.0f;
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
            trust = max(0.0f, trust - 10.0f); // Penalización severa por despertar brusco
            Serial.printf("[PET] ¡Despertar brusco (< 75%%)! Apego penalizado a: %.1f%%\n", trust);
            if (brain) {
                brain->emitReward(-0.8f);
            }
        } else {
            state = PetState::Idle;
        }
    } else if (!lightsOn && energy < 40.0f) {
        // Acostar a la mascota cuando está cansada construye apego
        trust = min(100.0f, trust + 2.0f);
    }
    save();
}

void Pet::forcePoop() {
    if (stage == PetStage::Egg || stage == PetStage::Dead || state == PetState::Dead) return;
    poopCount = min(3, poopCount + 1);
    poopExposureTimer = 0.0f;
    Serial.printf("[TEST] Caca añadida (+1). Total en suelo: %d/3\n", poopCount);
    save();
}

void Pet::drainEnergy(float amount) {
    if (stage == PetStage::Dead || state == PetState::Dead) return;
    energy = max(0.0f, energy - amount);
    Serial.printf("[TEST] Energía drenada a: %.1f\n", energy);
    save();
}

void Pet::makeSick() {
    if (stage == PetStage::Egg || stage == PetStage::Dead || state == PetState::Dead) return;
    health = 25.0f;
    state = PetState::Sick;
    actionTimer = 5.0f;
    if (brain) {
        static_cast<Brain*>(brain.get())->forceThought(THOUGHT_STRESS, "Enfermedad y dolor agudo inducidos");
    }
    Serial.printf("[TEST] Enfermedad inducida. Salud reducida a: %.1f%%\n", health);
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
    String path;

    // 1. Coincidencia exacta
    path = "/tama/sprites/base/" + species + "/" + folder + "/" + anim + ".png";
    if (SD_MMC.exists(path)) return path;

    // 2. Coincidencia exacta (base tiernito)
    path = "/tama/sprites/base/tiernito/" + folder + "/" + anim + ".png";
    if (SD_MMC.exists(path)) return path;

    // 3. Fallback a IDLE de la etapa actual
    path = "/tama/sprites/base/" + species + "/" + folder + "/idle.png";
    if (SD_MMC.exists(path)) return path;
    
    path = "/tama/sprites/base/tiernito/" + folder + "/idle.png";
    if (SD_MMC.exists(path)) return path;

    // 4. Fallback a CHILD si la etapa Adulto/Anciano aún no está dibujada
    path = "/tama/sprites/base/" + species + "/child/idle.png";
    if (SD_MMC.exists(path)) return path;

    path = "/tama/sprites/base/tiernito/child/idle.png";
    if (SD_MMC.exists(path)) return path;

    // 5. Fallback final universal a BEBE
    path = "/tama/sprites/base/" + species + "/bebe/idle.png";
    if (SD_MMC.exists(path)) return path;

    return "/tama/sprites/base/tiernito/bebe/idle.png";
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
    trust         = doc["trust"] | 5.0f;
    poopCount     = doc["poop"] | 0;
    lightsOn      = doc["lights"] | true;
    age           = doc["age"] | 0.0f;
    lastTimestamp = doc["lastTimestamp"] | 0;
    if (doc.containsKey("dna")) { // Cargar ADN o generar uno si es un perfil viejo
        dnaSeed = doc["dna"];
    } else {
        dnaSeed = esp_random(); 
    }

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
    doc["trust"]         = trust;
    doc["poop"]          = poopCount;
    doc["lights"]        = lightsOn;
    doc["age"]           = age;
    doc["lastTimestamp"] = (uint32_t)time(NULL);
    doc["dna"]           = dnaSeed; // Guardar ADN

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
    hunger = 100.0f; happiness = 100.0f; energy = 100.0f; health = 100.0f; trust = 5.0f;
    poopCount = 0; lightsOn = true; age = 0.0f; actionTimer = 0.0f; poopTimer = 0.0f;
    digestiveTransitTimer = 0.0f; digesting = false; poopExposureTimer = 0.0f;
    lastTimestamp = (uint32_t)time(NULL);
    dnaSeed = esp_random(); // Nuevo ADN al reiniciar partida
    save();
}

void Pet::evolveNextStage() {
    if (stage == PetStage::Dead || state == PetState::Dead) return;
    PetStage next = stage;
    if (stage == PetStage::Egg) next = PetStage::Baby;
    else if (stage == PetStage::Baby) next = PetStage::Child;
    else if (stage == PetStage::Child) next = PetStage::Adult;
    else if (stage == PetStage::Adult) next = PetStage::Senior;
    else {
        Serial.println("[PET] La mascota ya se encuentra en la etapa maxima (Senior).");
        return;
    }
    
    if (next == PetStage::Baby) age = TOTAL_LIFESPAN_SECONDS * EGG_RATIO + 1.0f;
    else if (next == PetStage::Child) age = TOTAL_LIFESPAN_SECONDS * (EGG_RATIO + BABY_RATIO) + 1.0f;
    else if (next == PetStage::Adult) age = TOTAL_LIFESPAN_SECONDS * (EGG_RATIO + BABY_RATIO + CHILD_RATIO) + 1.0f;
    else if (next == PetStage::Senior) age = TOTAL_LIFESPAN_SECONDS + 1.0f;

    stage = next;
    Serial.printf("[TEST] Evolucion forzada a: %s (Edad: %.0f s)\n", stageToString(stage).c_str(), age);
    save();
}

void Pet::jumpToStage(PetStage s) {
    stage = s;
    if (s == PetStage::Egg) age = 0.0f;
    else if (s == PetStage::Baby) age = TOTAL_LIFESPAN_SECONDS * EGG_RATIO + 1.0f;
    else if (s == PetStage::Child) age = TOTAL_LIFESPAN_SECONDS * (EGG_RATIO + BABY_RATIO) + 1.0f;
    else if (s == PetStage::Adult) age = TOTAL_LIFESPAN_SECONDS * (EGG_RATIO + BABY_RATIO + CHILD_RATIO) + 1.0f;
    else if (s == PetStage::Senior) age = TOTAL_LIFESPAN_SECONDS + 1.0f;
    Serial.printf("[TEST] Salto directo a etapa: %s\n", stageToString(stage).c_str());
    save();
}
