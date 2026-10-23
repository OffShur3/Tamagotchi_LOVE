// src/tamagotchi/OrphanageManager.h
#pragma once
#include <Arduino.h>
#include <vector>
#include "PetDef.h"
#include "SpeciesDef.h"

struct OrphanCandidate {
    String name;
    String species;
    PetStage stage;
    uint32_t dnaSeed;
    PetGenome genome;
    String brainType;
    float hunger;
    float trust;
};

class OrphanageManager {
public:
    static OrphanageManager& getInstance() {
        static OrphanageManager instance;
        return instance;
    }

    void init() {
        candidates.clear();
        timeSinceBoot = 0.0f;
        // Los niños tardan entre 45 y 60 segundos después de encender en llegar al orfanato
        arrivalDelay = 45.0f;
        hasArrived = false;
        generatingStep = 0;
    }

    void update(float dt) {
        if (hasArrived) return;

        timeSinceBoot += dt;
        // Generación lenta y gradual en segundo plano
        if (timeSinceBoot >= 15.0f && generatingStep == 0) {
            generatingStep = 1;
            candidates.push_back(generateCandidate("tiernito", PetStage::Baby));
        } else if (timeSinceBoot >= 30.0f && generatingStep == 1) {
            generatingStep = 2;
            candidates.push_back(generateCandidate("rebeldito", PetStage::Child));
        } else if (timeSinceBoot >= arrivalDelay && generatingStep == 2) {
            generatingStep = 3;
            candidates.push_back(generateCandidate("tiernito", PetStage::Child));
            hasArrived = true;
            Serial.printf("[ORFANATO] ¡Los niños han llegado! (%d candidatos disponibles para adopcion)\n", (int)candidates.size());
        }
    }

    bool isReady() const {
        return hasArrived && !candidates.empty();
    }

    int getSecondsRemaining() const {
        if (hasArrived) return 0;
        float rem = arrivalDelay - timeSinceBoot;
        return (rem > 0.0f) ? (int)(rem + 0.5f) : 1;
    }

    const std::vector<OrphanCandidate>& getCandidates() const {
        return candidates;
    }

    bool adoptCandidate(size_t index, String& outId) {
        if (index >= candidates.size()) return false;
        const auto& c = candidates[index];
        outId = Pet::spawnNewPet(c.species, c.stage, true);
        if (outId.length() > 0) {
            // Actualizar con los datos exactos del huérfano adoptado
            Pet temp;
            if (temp.loadById(outId)) {
                temp.setPetName(c.name);
                temp.selectBrainArchetype(c.brainType);
                temp.save();
            }
            candidates.erase(candidates.begin() + index);
            return true;
        }
        return false;
    }

private:
    OrphanageManager() = default;

    std::vector<OrphanCandidate> candidates;
    float timeSinceBoot = 0.0f;
    float arrivalDelay = 45.0f;
    bool hasArrived = false;
    int generatingStep = 0;

    OrphanCandidate generateCandidate(const String& sp, PetStage st) {
        OrphanCandidate c;
        c.species = sp;
        c.stage = st;
        c.dnaSeed = esp_random();

        SpeciesInfo sInfo = getSpeciesInfo(sp);
        auto randRange = [](float minV, float maxV) {
            if (maxV <= minV) return minV;
            uint32_t span = (uint32_t)((maxV - minV) * 100.0f);
            return minV + (esp_random() % (span + 1)) / 100.0f;
        };

        c.genome.metabolismRate = randRange(sInfo.metabMin, sInfo.metabMax);
        c.genome.socialNeed     = randRange(sInfo.socialMin, sInfo.socialMax);
        c.genome.resilience     = randRange(sInfo.resMin, sInfo.resMax);
        c.genome.sleepPacing    = randRange(sInfo.sleepMin, sInfo.sleepMax);

        // Arquetipos de personalidad humana
        if (c.genome.socialNeed > 1.15f) c.genome.personalityTag = "Mimoso y sensible";
        else if (c.genome.socialNeed < 0.90f) c.genome.personalityTag = "Independiente";
        else if (c.genome.metabolismRate > 1.15f) c.genome.personalityTag = "Glotoncito";
        else c.genome.personalityTag = "Tranquilo";

        static const char* brains[] = { "LEARNER", "STD", "HYPER", "DUMB" };
        c.brainType = brains[random(0, 4)];

        static const char* orphanNames[] = { "Pupi", "Milo", "Luna", "Toby", "Ciro", "Nina", "Kiki", "Bobi" };
        c.name = orphanNames[random(0, 8)];

        // Huérfanos: hambre moderada y apego bajísimo porque no conocen al jugador
        c.hunger = (float)random(30, 60);
        c.trust  = (float)random(0, 4);

        return c;
    }
};
