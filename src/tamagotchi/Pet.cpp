#include "Pet.h"
#include <SD_MMC.h>
#include <ArduinoJson.h>

struct SpiRamAllocator {
    void* allocate(size_t size) { void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); return p ? p : malloc(size); }
    void deallocate(void* pointer) { free(pointer); }
    void* reallocate(void* ptr, size_t new_size) { void* p = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); return p ? p : realloc(ptr, new_size); }
};
typedef BasicJsonDocument<SpiRamAllocator> SpiRamJsonDocument;

Pet::Pet() {}

void Pet::init() {
    if (!load()) {
        Serial.println("[PET] No se encontró partida. Inicializando huevo...");
        reset();
    }
    catchUpTime();
}

void Pet::catchUpTime() {
    time_t now = time(NULL);
    if (lastTimestamp > 0 && now > lastTimestamp) {
        uint32_t elapsed = (uint32_t)(now - lastTimestamp);

        // PROTECCIÓN CONTRA SALTO DE FECHA NTP (Ej. de Enero a Julio)
        // Si el salto es mayor a 8 horas (28800s), limitamos el cálculo a 8 horas max.
        if (elapsed > 28800) { 
            Serial.printf("[PET] Salto de fecha detectado (%u s). Capando avance a 8 horas max.\n", elapsed);
            elapsed = 28800; 
        }

        if (elapsed > 2) {
            Serial.printf("[PET] Procesando %u segundos transcurridos fuera de línea...\n", elapsed);
            
            // Simular el tiempo en bloques de 60 segundos para una degradación suave
            uint32_t remaining = elapsed;
            while (remaining > 0) {
                float step = (remaining > 60) ? 60.0f : (float)remaining;
                update(step);
                remaining -= (uint32_t)step;
            }
            save(); // Guardar resultado final una sola vez
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
    Serial.printf("  Edad:        %u segs (%.1f%% de vida alcanzado)\n", age, lifePercent);
    Serial.printf("  Ciclo Config: %.0f segs (%.1f hrs / %.1f días)\n", 
                  TOTAL_LIFESPAN_SECONDS, TOTAL_LIFESPAN_SECONDS / 3600.0f, TOTAL_LIFESPAN_SECONDS / 86400.0f);
    Serial.println("=================================\n");
}

void Pet::update(float dt) {
    if (stage == PetStage::Dead) return;

    age += dt;

    // 1. AUTO-GUARDADO CADA 30 SEGUNDOS (Solo si no estamos en catch-up masivo)
    if (dt < 2.0f) { // Evita spams si dt es grande
        autoSaveTimer += dt;
        if (autoSaveTimer >= 30.0f) {
            autoSaveTimer = 0.0f;
            save();
            Serial.println("[PET] Auto-guardado de rutina en SD (30s).");
        }
    }

    // 2. Temporizador de animaciones táctiles
    if (actionTimer > 0.0f) {
        actionTimer -= dt;
        if (actionTimer <= 0.0f && state != PetState::Dead) {
            state = PetState::Idle;
        }
    }

    // 3. EVOLUCIÓN
    checkEvolution();

    // 4. DESGASTE DE ESTADÍSTICAS
    if (stage != PetStage::Egg) {
        float stageHungerMult = (stage == PetStage::Baby) ? 1.5f : 1.0f;
        float stageEnergyMult = (stage == PetStage::Baby || stage == PetStage::Senior) ? 1.4f : 1.0f;

        float hungerRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.06f)) * stageHungerMult;
        hunger = max(0.0f, hunger - (hungerRate * dt));

        float happinessRate = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.10f));
        happiness = max(0.0f, happiness - (happinessRate * dt));

        if (!lightsOn) {
            float energyGain = (100.0f / (TOTAL_LIFESPAN_SECONDS * 0.02f));
            energy = min(100.0f, energy + (energyGain * dt));
            state = PetState::Sleeping;
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

    checkStateTransitions();
}

void Pet::checkEvolution() {
    if (stage == PetStage::Dead) return;

    float tEgg    = TOTAL_LIFESPAN_SECONDS * EGG_RATIO;
    float tBaby   = tEgg  + (TOTAL_LIFESPAN_SECONDS * BABY_RATIO);
    float tChild  = tBaby + (TOTAL_LIFESPAN_SECONDS * CHILD_RATIO);
    float tAdult  = tChild + (TOTAL_LIFESPAN_SECONDS * ADULT_RATIO);
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
    if (health <= 0.0f) { stage = PetStage::Dead; state = PetState::Dead; return; }
    if (actionTimer > 0.0f) return;

    if (!lightsOn) { state = PetState::Sleeping; return; }
    if (health < 40.0f) { state = PetState::Sick; }
    else if (happiness < 30.0f) { state = PetState::Sad; }
    else { state = PetState::Idle; }
}

void Pet::feed() {
    if (stage == PetStage::Egg || stage == PetStage::Dead) return;
    hunger = min(100.0f, hunger + 35.0f);
    state = PetState::Eating;
    actionTimer = 2.5f;
    save();
}

void Pet::pet() {
    if (stage == PetStage::Egg) { 
        // 1 toque acelera el 1% de la vida total
        age += (TOTAL_LIFESPAN_SECONDS * 0.005f); 
        Serial.println("[PET] ¡Tocaste el huevo! Acelerando eclosión...");
        return; 
    }
    if (stage == PetStage::Dead) return;
    happiness = min(100.0f, happiness + 25.0f);
    state = PetState::Happy;
    actionTimer = 2.0f;
    save();
}

void Pet::heal() {
    if (stage == PetStage::Egg || stage == PetStage::Dead) return;
    health = min(100.0f, health + 60.0f);
    state = PetState::Happy;
    actionTimer = 2.0f;
    save();
}

void Pet::clean() {
    if (poopCount > 0) {
        poopCount = 0;
        poopTimer = 0.0f;
        happiness = min(100.0f, happiness + 15.0f);
        Serial.println("[PET] ¡Suelo limpiado!");
        save();
    }
}

void Pet::toggleLights() {
    lightsOn = !lightsOn;
    Serial.printf("[PET] Luz %s\n", lightsOn ? "Encendida" : "Apagada");
    save();
}

String Pet::getSpritePath() const {
    String folder = stageToString(stage);
    String anim   = stateToString(state);
    String basePath = "/tama/sprites/base/" + species + "/" + folder + "/" + anim;

    if (SD_MMC.exists(basePath + ".png")) return basePath + ".png";
    if (SD_MMC.exists(basePath)) return basePath;
    return "/tama/sprites/base/tiernito/" + folder + "/" + anim + ".png";
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
