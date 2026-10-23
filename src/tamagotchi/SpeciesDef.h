// src/tamagotchi/SpeciesDef.h
#pragma once
#include <Arduino.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>

struct SpeciesInfo {
    String id = "tiernito";
    String displayName = "Tiernito";
    String lore = "Raza dulce y muy apegada a su dueno.";
    float metabMin = 0.85f, metabMax = 1.15f;
    float socialMin = 1.10f, socialMax = 1.35f; // Sesgo cariñoso por defecto
    float resMin = 0.90f, resMax = 1.20f;
    float sleepMin = 0.95f, sleepMax = 1.25f;
};

inline SpeciesInfo getSpeciesInfo(const String& speciesId) {
    SpeciesInfo info;
    info.id = speciesId;
    info.displayName = speciesId;
    if (info.displayName.length() > 0) {
        info.displayName[0] = toupper(info.displayName[0]);
    }

    // Si creas una carpeta nueva en /tama/sprites/base/<raza>/species.json, lee sus sesgos de ahí
    String jsonPath = "/tama/sprites/base/" + speciesId + "/species.json";
    if (SD_MMC.exists(jsonPath)) {
        File f = SD_MMC.open(jsonPath, "r");
        if (f) {
            StaticJsonDocument<512> doc;
            if (!deserializeJson(doc, f)) {
                info.displayName = doc["name"] | info.displayName;
                info.lore        = doc["lore"] | info.lore;
                info.metabMin    = doc["metabMin"]  | 0.85f;
                info.metabMax    = doc["metabMax"]  | 1.20f;
                info.socialMin   = doc["socialMin"] | 0.85f;
                info.socialMax   = doc["socialMax"] | 1.20f;
                info.resMin      = doc["resMin"]    | 0.85f;
                info.resMax      = doc["resMax"]    | 1.20f;
                info.sleepMin    = doc["sleepMin"]  | 0.85f;
                info.sleepMax    = doc["sleepMax"]  | 1.20f;
            }
            f.close();
            return info;
        }
    }

    // Sesgos predefinidos en código mientras vas dibujando las razas en pixelart
    if (speciesId == "tiernito") {
        info.displayName = "Tiernito";
        info.lore = "Suelen ser muy carinosos, dulces y buscan compania.";
        info.socialMin = 1.10f; info.socialMax = 1.35f;
        info.metabMin  = 0.90f; info.metabMax  = 1.15f;
    } else if (speciesId == "rebeldito") {
        info.displayName = "Rebeldito";
        info.lore = "Suelen ser desapegados, independientes y muy fuertes.";
        info.socialMin = 0.65f; info.socialMax = 0.88f;
        info.resMin    = 1.10f; info.resMax    = 1.35f;
    } else if (speciesId == "glotonsito") {
        info.displayName = "Glotonsito";
        info.lore = "Tienen un metabolismo voraz y les encanta dormir.";
        info.metabMin  = 1.15f; info.metabMax  = 1.40f;
        info.sleepMin  = 1.10f; info.sleepMax  = 1.35f;
    }
    return info;
}
