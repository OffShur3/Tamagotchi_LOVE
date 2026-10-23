// src/tamagotchi/Pet.h
#pragma once
#include "PetDef.h"
#include "brain/IBrain.h"
#include "brain/Brain.h"
#include <Arduino.h>
#include <memory>
#include <vector>

constexpr float TOTAL_LIFESPAN_SECONDS = 1036800.0f; // <-- CAMBIO: 12 días reales de vida

constexpr float EGG_RATIO    = 0.001f; // ~17 minutos
constexpr float BABY_RATIO   = 0.049f; // ~14 horas
constexpr float CHILD_RATIO  = 0.200f; // ~2.4 días
constexpr float ADULT_RATIO  = 0.500f; // ~6 días
constexpr float SENIOR_RATIO = 0.250f; // ~3 días

class Pet {
public:
    Pet();

    void init();
    void update(float dt);
    void catchUpTime();

    void feed();
    void pet();
    void heal();
    void clean();
    void toggleLights();

    void forceState(PetState newState) {
        state = newState;
        if (newState == PetState::Dead) {
            health = 0.0f;
            actionTimer = 0.0f; 
            Serial.printf("[PET-ALERT] Mascota fallecida. Etapa: %s | Mostrando %s/dead.png\n", 
                          stageToString(stage).c_str(), stageToString(stage).c_str());
        } else {
            actionTimer = 5.0f; 
            Serial.printf("[DEBUG] Estado forzado a: %s (5 segundos)\n", stateToString(newState).c_str());
        }
        save();
    }

    // Suite de pruebas
    void forcePoop();
    void maxPoop() { poopCount = 3; poopTimer = 0.0f; save(); }
    void drainEnergy(float amount = 50.0f);
    void makeSick();
    void starve() { hunger = 0.0f; state = PetState::Sad; save(); }
    void makeBored() { happiness = 10.0f; state = PetState::Sad; save(); }
    void makeHappy() { happiness = 100.0f; if (brain) brain->emitReward(1.0f); save(); }
    void godMode() {
        hunger = 100.0f; happiness = 100.0f; energy = 100.0f; health = 100.0f;
        poopCount = 0; trust = 100.0f; state = PetState::Idle; save();
        Serial.println("[TEST] Modo Dios activado: 100% en todas las estadisticas.");
    }
    void revive() {
        state = PetState::Idle;
        health = 100.0f; energy = 100.0f; hunger = 100.0f; happiness = 100.0f;
        poopCount = 0; trust = 50.0f;
        if (stage == PetStage::Dead) stage = PetStage::Baby;
        Serial.println("[TEST] Mascota revivida con exito!");
        save();
    }
    void evolveNextStage();
    void jumpToStage(PetStage s);
    void accelerateAge(float seconds = 3600.0f);
    void fastForwardHours(float hours) {
        if (hours <= 0.0f || hours > 48.0f) return;
        uint32_t totalSec = (uint32_t)(hours * 3600.0f);
        Serial.printf("\n[TIME-WARP] Simulando %.1f horas (%u s) con Luz %s...\n",
                      hours, totalSec, lightsOn ? "PRENDIDA" : "APAGADA");
        uint32_t rem = totalSec;
        while (rem > 0) {
            float step = (rem > 30) ? 30.0f : (float)rem;
            update(step);
            rem -= (uint32_t)step;
        }
        save();
        printStats();
    }

    void rollGenetics();
    const PetGenome& getGenome() const { return genome; }

    void setBrain(std::unique_ptr<IBrain> newBrain);
    void selectBrainArchetype(const String& archetypeName);
    IBrain* getBrain() const { return brain.get(); }

    void printStats() const;
    int getThoughtIcon() const {
        return brain ? brain->getActiveThought() : -1;
    }

    bool load();
    bool loadById(const String& id);
    bool save();
    void reset();

    String getPetId() const { return petId; }
    void setPetId(const String& id) { petId = id; }
    String getPetName() const { return petName; }
    void setPetName(const String& n) { petName = n; save(); }
    String getSaveFilePath() const { return "/config/pets/" + petId + ".json"; }

    static void migrateLegacySaveIfNeeded();
    static String getActivePetId();
    static void setActivePetId(const String& id);
    static std::vector<String> listSavedPetIds();
    static String spawnNewPet(const String& speciesName = "tiernito", PetStage initialStage = PetStage::Baby, bool lowTrustOrphan = false);

    String getSpecies() const { return species; }
    std::vector<String> discoverInstalledSpecies();

    PetStage getStage() const { return stage; }
    PetState getState() const { return state; }
    float getHunger() const { return hunger; }
    float getHappiness() const { return happiness; }
    float getEnergy() const { return energy; }
    float getHealth() const { return health; }
    float getTrust() const { return trust; }
    uint8_t getPoopCount() const { return poopCount; }
    bool isLightOn() const { return lightsOn; }
    bool isBedtimeHours() const;
    uint32_t getAge() const { return (uint32_t)age; }
    uint32_t getDnaSeed() const { return dnaSeed; }

    String getSpritePath() const;
    String getWalkSpritePath() const;
    static String resolveSpritePathFor(const String& sp, PetStage st, const String& animName);

private:
    String petId = "pet_001";
    String petName = "Tiernito #1";
    String species = "tiernito";
    String accHat = "";
    String accFace = "";
    PetStage stage = PetStage::Egg;
    PetState state = PetState::Idle;
    uint32_t dnaSeed = 0;
    PetGenome genome;

    float hunger = 100.0f;
    float happiness = 100.0f;
    float energy = 100.0f;
    float health = 100.0f;
    float trust = 5.0f; // Apego / Confianza (0.0f a 100.0f)

    uint8_t poopCount = 0;
    bool lightsOn = true;

    float age = 0.0f;
    uint32_t lastTimestamp = 0;
    
    float actionTimer = 0.0f;
    float poopTimer = 0.0f;
    float autoSaveTimer = 0.0f;

    // Motor Digestivo Biológico y Control de Sueño / Abandono
    float digestiveTransitTimer = 0.0f;
    bool digesting = false;
    float poopExposureTimer = 0.0f;
    float unattendedSleepTimer = 0.0f;
    bool abandonmentPoopTriggered = false;

    std::unique_ptr<IBrain> brain;

    void checkStateTransitions();
    void checkEvolution();
};
