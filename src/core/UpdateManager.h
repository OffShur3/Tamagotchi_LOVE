// src/core/UpdateManager.h
#ifndef UPDATE_MANAGER_H
#define UPDATE_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <SD_MMC.h>
#include <Arduino_GFX_Library.h>

class UpdateManager {
public:
    struct Config {
        Arduino_GFX* gfx;
        const char* githubUser;          // Ejemplo: "OffShur3"
        const char* githubRepo;          // Ejemplo: "Tamagotchi_LOVE"
        const char* currentVersionPath;  // Ejemplo: "/version.txt"
        uint32_t checkIntervalMs;        // Tiempo entre comprobaciones
    };

    UpdateManager(const Config& config);

    bool checkForUpdate();
    bool needMandatoryUpdate();
    String getCurrentVersion();
    String getLatestVersion() const { return _latestVersion; }
    bool isUpdateAvailable() const { return _updateAvailable; }

    bool performFullUpdate(); 
    bool performFirmwareUpdate();
    bool performSDUpdate();

private:
    Config _cfg;
    String _latestVersion;
    bool _updateAvailable;

    String getAssetUrl(const char* assetName);
    void drawProgress(const String& title, int progress, int total);
    void extractTar(const char* tarPath, const char* destDir);
    void writeVersionFile(const String& version);
};

#endif
