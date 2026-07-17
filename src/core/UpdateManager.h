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
        const char* githubUser;          // "OffShur3"
        const char* githubRepo;          // "Tamagotchi_LOVE"
        const char* currentVersionPath;  // "/version.txt"
        uint32_t checkIntervalMs;        // 300000
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
    String _lastTitle; // Control anti-parpadeo para la interfaz

    String getAssetUrl(const char* assetName);
    void drawMessage(const String& msg);
    void drawProgress(const String& title, int progress, int total);
    void extractTar(const char* tarPath, const char* destDir);
    void writeVersionFile(const String& version);
};

#endif
