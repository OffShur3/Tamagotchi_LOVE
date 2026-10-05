// src/core/PlayerManager.cpp
#include "PlayerManager.h"
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include <time.h>

bool PlayerManager::init() {
    SD_MMC.mkdir("/config");
    return load();
}

bool PlayerManager::hasProfile() const {
    return profileLoaded && (ownerName.length() >= 2);
}

String PlayerManager::getOwnerName() const {
    return (ownerName.length() >= 2) ? ownerName : "Humano";
}

void PlayerManager::setOwnerName(const String& name) {
    ownerName = name;
    ownerName.trim();
    profileLoaded = (ownerName.length() >= 2);
    save();
}

bool PlayerManager::load() {
    if (!SD_MMC.exists(CONFIG_PATH)) {
        profileLoaded = false;
        ownerName = "";
        return false;
    }

    File f = SD_MMC.open(CONFIG_PATH, "r");
    if (!f) return false;

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.println("[PLAYER] Error parseando /config/player.json");
        profileLoaded = false;
        return false;
    }

    ownerName = doc["ownerName"] | "";
    ownerName.trim();
    createdTimestamp = doc["createdTimestamp"] | 0;

    profileLoaded = (ownerName.length() >= 2);
    if (profileLoaded) {
        Serial.printf("[PLAYER] Perfil cargado. Dueño: \"%s\"\n", ownerName.c_str());
    }
    return profileLoaded;
}

bool PlayerManager::save() {
    if (ownerName.length() < 2) return false;

    SD_MMC.mkdir("/config");
    File f = SD_MMC.open(CONFIG_PATH, "w");
    if (!f) {
        Serial.println("[PLAYER] Error escribiendo /config/player.json");
        return false;
    }

    StaticJsonDocument<512> doc;
    doc["ownerName"] = ownerName;
    if (createdTimestamp == 0) {
        createdTimestamp = (uint32_t)time(NULL);
    }
    doc["createdTimestamp"] = createdTimestamp;

    serializeJson(doc, f);
    f.close();

    profileLoaded = true;
    Serial.printf("[PLAYER] Perfil guardado con exito: \"%s\"\n", ownerName.c_str());
    return true;
}

void PlayerManager::reset() {
    ownerName = "";
    createdTimestamp = 0;
    profileLoaded = false;
    if (SD_MMC.exists(CONFIG_PATH)) {
        SD_MMC.remove(CONFIG_PATH);
    }
}
