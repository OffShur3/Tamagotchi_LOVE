// src/tamagotchi/Pet.cpp
#include "Pet.h"
#include "brain/BrainArchetypes.h"
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
        brain->applyGenome(genome);
        Serial.printf("[PET] Transplante de cerebro exitoso! Nuevo cerebro: %s\n", brain->getName());
    }
}

void Pet::selectBrainArchetype(const String& archetypeName) {
    setBrain(createBrainArchetype(archetypeName));
    save();
}

void Pet::init() {
    if (!load()) {
        Serial.println("[PET] No se encontró partida. Inicializando huevo...");
        reset();
    }
    catchUpTime();
}

bool Pet::isBedtimeHours() const {
    time_t now = time(NULL);
    if (now < 1700000000) return false;
    struct tm tInfo;
    localtime_r(&now, &tInfo);
    return (tInfo.tm_hour >= 22 || tInfo.tm_hour < 4);
}

void Pet::catchUpTime() {
    uint32_t now = (uint32_t)time(NULL);
    if (stage == PetStage::Dead || state == PetState::Dead) return;

    // Soporta tanto reloj sincronizado por NTP (>= 1700000000) como reloj RTC interno en Deep Sleep Offline
    bool bothSynced  = (now >= 1700000000 && lastTimestamp >= 1700000000);
    bool bothOffline = (now < 1700000000 && lastTimestamp > 0 && lastTimestamp < 1700000000);

    if ((bothSynced || bothOffline) && now > lastTimestamp) {
        uint32_t elapsed = now - lastTimestamp;

        // Simula hasta 48 horas (172800 s) de Deep Sleep o apagado real
        if (elapsed > 2 && elapsed <= 172800) {
            Serial.printf("[PET] Procesando %u segundos (%.2f horas) de Deep Sleep / Offline...\n", elapsed, elapsed / 3600.0f);
            uint32_t remaining = elapsed;
            while (remaining > 0) {
                float step = (remaining > 30) ? 30.0f : (float)remaining;
                update(step);
                remaining -= (uint32_t)step;
            }
            save();
        } else if (elapsed > 172800) {
            Serial.printf("[PET] Salto mayor a 48h detectado (%u s). Sincronizando reloj sin penalizacion.\n", elapsed);
            save();
        }
    }
    lastTimestamp = now;
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
    Serial.printf("  Genoma:      Metab:%.2f | Soc:%.2f | Res:%.2f | Slp:%.2f\n", genome.metabolismRate, genome.socialNeed, genome.resilience, genome.sleepPacing);
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
                0.0f, 0.0f, 0.0f, 0.0f, poopCount, lightsOn, false, stage, false, trust, false 
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
        float stageHungerMult = (stage == PetStage::Baby) ? 1.25f : 1.0f;
        float stageEnergyMult = (stage == PetStage::Baby || stage == PetStage::Senior) ? 1.25f : 1.0f;

        float hungerCognitiveMult = brain ? brain->getHungerMetabolicRate() : 1.0f;
        float energyCognitiveMult = brain ? brain->getEnergyDrainMultiplier() : 1.0f;
        float stressCognitiveMult = brain ? brain->getStressMultiplier() : 1.0f;

        if (!lightsOn) {
            // ================= MODO SUEÑO (LUZ APAGADA - OPCIÓN A) =================
            state = PetState::Sleeping;
            unattendedSleepTimer += dt;

            // 1. Energía: Se recupera al 100% en ~6.5 horas (23400 s) y sigue durmiendo en paz
            float energyGainPerSec = 100.0f / 23400.0f;
            energy = min(100.0f, energy + (energyGainPerSec * dt));

            // 2. Hambre Nocturna:
            // - Primeras 10h (hunger >= 25%): baja 7.5%/h -> a las 10h despierta con ~25% de hambre
            // - De 10h a 12h (hunger < 25%): baja 10.0%/h -> a las 12h llega a ~5% (hambre crítica)
            float sleepHungerPerHour = (hunger >= 25.0f) ? 7.5f : 10.0f;
            hunger = max(0.0f, hunger - ((sleepHungerPerHour / 3600.0f) * dt));

            // 3. Felicidad Nocturna:
            // - Primeras 10h: baja 5.5%/h -> a las 10h queda en ~45%
            // - Pasadas las 10h: baja 12.0%/h por angustia de soledad
            float sleepHappyPerHour = (unattendedSleepTimer <= 36000.0f) ? 5.5f : 12.0f;
            happiness = max(0.0f, happiness - ((sleepHappyPerHour / 3600.0f) * dt));

            // 4. Descuido Nocturno (> 10.5h - 12h o hambre < 12%): Miedo al abandono y caca por susto
            bool isAbandonedInSleep = (unattendedSleepTimer > 37800.0f || hunger < 12.0f);
            if (isAbandonedInSleep) {
                if (!abandonmentPoopTriggered && (unattendedSleepTimer > 39600.0f || hunger <= 8.0f)) {
                    abandonmentPoopTriggered = true;
                    if (poopCount < 3) {
                        poopCount++;
                        poopExposureTimer = 0.0f;
                    }
                    if (brain) {
                        brain->forceThought(THOUGHT_STRESS, "Susto por abandono: Penso que no volverias a alimentarlo");
                    }
                    Serial.println("[PET] ¡Descuido prolongado (>11h)! Hizo caca por tiempo y miedo al abandono.");
                }

                // Pérdida de Salud (~15%/h) y Apego (~6%/h) tras superar el límite de las 10.5h
                float abandonHealthDrop = (15.0f / 3600.0f) * (1.0f + (poopCount * 0.4f)) * stressCognitiveMult;
                health = max(0.0f, health - (abandonHealthDrop * dt));
                trust  = max(0.0f, trust  - ((6.0f / 3600.0f) * dt));
            }
        } else {
            // ================= MODO VIGILIA / DÍA (LUZ PRENDIDA) =================
            unattendedSleepTimer = 0.0f;
            abandonmentPoopTriggered = false;

            // 1. Hambre Diurna: 100% a 0% en ~5.5 horas (19800 s -> ~18.18%/h)
            float awakeHungerRate = (100.0f / 19800.0f) * stageHungerMult * hungerCognitiveMult;
            hunger = max(0.0f, hunger - (awakeHungerRate * dt));

            // 2. Felicidad Diurna: 100% a 0% en ~8.0 horas (28800 s -> 12.5%/h)
            float awakeHappyRate = (100.0f / 28800.0f);
            happiness = max(0.0f, happiness - (awakeHappyRate * dt));

            // 3. Energía Diurna: 100% a 0% en ~15.0 horas (54000 s -> ~6.67%/h)
            float awakeEnergyRate = (100.0f / 54000.0f) * stageEnergyMult * energyCognitiveMult;
            energy = max(0.0f, energy - (awakeEnergyRate * dt));

            // 4. Motor Digestivo Biológico (Activo estando despierto)
            if (digesting) {
                if (hunger < 15.0f) {
                    digesting = false;
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

            // 5. Penalización por negligencia estando despierto
            if (poopCount > 0) {
                poopExposureTimer += dt;
                if (poopExposureTimer > 900.0f && brain) {
                    brain->stimulateNeuron(3, 0.08f * dt);
                }
                if (poopExposureTimer > 1200.0f) {
                    trust = max(0.0f, trust - (0.2f * dt / 60.0f));
                }
            }

            if (hunger < 12.0f) {
                // Si pasa >5 horas despierto sin comer (hambre < 12%), empieza a asustarse y perder apego
                trust = max(0.0f, trust - (0.35f * dt / 60.0f));
            } else if (health < 30.0f && state == PetState::Sick) {
                trust = max(0.0f, trust - (0.30f * dt / 60.0f));
            }

            if (poopCount > 0 || hunger <= 5.0f || happiness <= 0.0f) {
                float healthDropRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.02f)) * (poopCount + 1) * stressCognitiveMult;
                health = max(0.0f, health - (healthDropRate * dt));
            }
        }
    }

    if (brain) {
        PetSensoryInput input = {
            hunger, energy, happiness, health, poopCount, lightsOn,
            (physicalTouchTimer > 0.0f),
            stage,
            (stage == PetStage::Egg),
            trust,
            isBedtimeHours()
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
            
            rollGenetics(); // Rolear genoma al nacer
            if (brain) brain->applyGenome(genome);
            
            Serial.printf("[PET] ¡Eclosion! Raza asignada: %s | Personalidad: %s\n", species.c_str(), genome.personalityTag.c_str());
        }

        stage = newStage;
        Serial.printf("[PET] ¡Evolucion! Nueva etapa: %s\n", stageToString(stage).c_str());
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
    } else if (!lightsOn) {
        // Se va a dormir inmediatamente aunque tenga mucha energía
        state = PetState::Sleeping;
        unattendedSleepTimer = 0.0f;
        abandonmentPoopTriggered = false;
        // Bono de apego si tenía sueño (< 50%) o si lo acostaste en su horario habitual (22h a 04h)
        if (energy < 50.0f || isBedtimeHours()) {
            trust = min(100.0f, trust + 2.0f);
        }
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
        brain->forceThought(THOUGHT_STRESS, "Enfermedad y dolor agudo inducidos");
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

    SpiRamJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    species                  = doc["species"] | "tiernito";
    stage                    = static_cast<PetStage>(doc["stage"] | 0);
    state                    = static_cast<PetState>(doc["state"] | 0);
    hunger                   = doc["hunger"] | 100.0f;
    happiness                = doc["happiness"] | 100.0f;
    energy                   = doc["energy"] | 100.0f;
    health                   = doc["health"] | 100.0f;
    trust                    = doc["trust"] | 5.0f;
    poopCount                = doc["poop"] | 0;
    lightsOn                 = doc["lights"] | true;
    age                      = doc["age"] | 0.0f;
    lastTimestamp            = doc["lastTimestamp"] | 0;
    unattendedSleepTimer     = doc["sleepTimer"] | 0.0f;
    abandonmentPoopTriggered = doc["abanPoop"] | false;

    if (doc.containsKey("dna")) {
        dnaSeed = doc["dna"];
    } else {
        dnaSeed = esp_random(); 
    }

    // 1. Cargar Genoma
    if (doc.containsKey("g_metab")) {
        genome.metabolismRate = doc["g_metab"];
        genome.socialNeed     = doc["g_soc"];
        genome.resilience     = doc["g_res"];
        genome.sleepPacing    = doc["g_slp"];
        genome.personalityTag = doc["g_tag"].as<String>();
    } else {
        if (stage != PetStage::Egg) {
            rollGenetics();
        } else {
            genome = PetGenome();
        }
    }

    // 2. Restaurar Arquetipo y Estado Completo del Cerebro (Neuronas + Sinapsis + Dopamina)
    String bType = doc["brain_type"] | "STD";
    brain = createBrainArchetype(bType);
    if (brain) {
        brain->init();
        brain->applyGenome(genome);
        if (doc.containsKey("brain_state")) {
            brain->loadState(doc["brain_state"].as<JsonObjectConst>());
            Serial.printf("[PET] Estado cerebral restaurado desde SD (Tipo: %s | %s)\n",
                          bType.c_str(), brain->getName());
        }
    }

    return true;
}

bool Pet::save() {
    SpiRamJsonDocument doc(8192);
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
    doc["sleepTimer"]    = unattendedSleepTimer;
    doc["abanPoop"]      = abandonmentPoopTriggered;
    doc["dna"]           = dnaSeed;
    doc["g_metab"]       = genome.metabolismRate;
    doc["g_soc"]         = genome.socialNeed;
    doc["g_res"]         = genome.resilience;
    doc["g_slp"]         = genome.sleepPacing;
    doc["g_tag"]         = genome.personalityTag;

    if (brain) {
        doc["brain_type"] = brain->getArchetypeId();
        JsonObject brainObj = doc.createNestedObject("brain_state");
        brain->saveState(brainObj);
    }

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
    unattendedSleepTimer = 0.0f; abandonmentPoopTriggered = false;
    lastTimestamp = (uint32_t)time(NULL);
    dnaSeed = esp_random(); // Nuevo ADN al reiniciar partida
    genome = PetGenome(); 
    if (brain) {
        brain->init();
        brain->applyGenome(genome);
    }
    save();
}

void Pet::rollGenetics() {
    auto randFloat = []() { return 0.8f + (esp_random() % 51) / 100.0f; }; // Rango 0.8 a 1.3
    
    genome.metabolismRate = randFloat();
    genome.socialNeed = randFloat();
    genome.resilience = randFloat();
    genome.sleepPacing = randFloat();

    float maxTrait = genome.metabolismRate;
    if (genome.socialNeed > maxTrait) maxTrait = genome.socialNeed;
    if (genome.resilience > maxTrait) maxTrait = genome.resilience;
    if (genome.sleepPacing > maxTrait) maxTrait = genome.sleepPacing;

    float minTrait = genome.metabolismRate;
    if (genome.socialNeed < minTrait) minTrait = genome.socialNeed;
    if (genome.resilience < minTrait) minTrait = genome.resilience;
    if (genome.sleepPacing < minTrait) minTrait = genome.sleepPacing;

    if (maxTrait == genome.metabolismRate && maxTrait > 1.15f) {
        genome.personalityTag = "Gloton";
    } else if (maxTrait == genome.socialNeed && maxTrait > 1.15f) {
        genome.personalityTag = "Mimoso y pegajoso";
    } else if (minTrait == genome.socialNeed && minTrait < 0.9f) {
        genome.personalityTag = "Independiente y callado";
    } else if (minTrait == genome.resilience && minTrait < 0.9f) {
        genome.personalityTag = "Ansioso y quejumbroso";
    } else {
        genome.personalityTag = "Alegre y normal";
    }
    
    Serial.printf("[PET-GENOME] Genetica roleada: %s (Metab: %.2f, Soc: %.2f, Res: %.2f, Slp: %.2f)\n", 
                  genome.personalityTag.c_str(), genome.metabolismRate, genome.socialNeed, genome.resilience, genome.sleepPacing);
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
