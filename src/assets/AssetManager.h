// src/assets/AssetManager.h
#pragma once
#include "Texture.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <FS.h>
#include <PNGdec.h>

class AssetManager {
public:
    bool loadPNGDirectToBuffer(const std::string& path, uint16_t* destBuffer);
    static AssetManager& getInstance() {
        static AssetManager instance;
        return instance;
    }

    void setFileSystem(fs::FS* fs) { fileSystem = fs; }
    void setPNG(PNG* p) { png = p; }
    
    // Asignación de DNA Seed para generación procedural
    void setActiveDnaSeed(uint32_t seed) { activeDnaSeed = seed; }
    uint32_t getActiveDnaSeed() const { return activeDnaSeed; }

    std::shared_ptr<Texture> getTexture(const std::string& filepath);
    
    void clearUnused();
    void clearAll() { cache.clear(); } 

private:
    AssetManager() : fileSystem(nullptr), png(nullptr), activeDnaSeed(0) {}
    
    fs::FS* fileSystem;
    PNG* png;
    uint32_t activeDnaSeed; // <-- NUEVO
    std::unordered_map<std::string, std::shared_ptr<Texture>> cache;

    std::shared_ptr<Texture> loadFromFile(const std::string& path);
};
