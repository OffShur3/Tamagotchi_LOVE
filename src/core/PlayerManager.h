// src/core/PlayerManager.h
#pragma once
#include <Arduino.h>

class PlayerManager {
public:
    static PlayerManager& getInstance() {
        static PlayerManager instance;
        return instance;
    }

    bool init();
    bool hasProfile() const;
    String getOwnerName() const;
    void setOwnerName(const String& name);
    bool save();
    bool load();
    void reset();

private:
    PlayerManager() = default;
    String ownerName = "";
    uint32_t createdTimestamp = 0;
    bool profileLoaded = false;
    static constexpr const char* CONFIG_PATH = "/config/player.json";
};
