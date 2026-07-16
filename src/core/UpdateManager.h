#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
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
        uint32_t checkIntervalMs;        // Tiempo entre comprobaciones
    };

    UpdateManager(const Config& config);

    bool checkForUpdate();
    bool needMandatoryUpdate();
    String getCurrentVersion();
    String getLatestVersion() const { return _latestVersion; }
    bool isUpdateAvailable() const { return _updateAvailable; }

    void drawUpdateBadge(int x, int y, int radius);
    bool isTouchingBadge(uint16_t touchX, uint16_t touchY, int badgeX, int badgeY, int radius);

    bool performFullUpdate(); 
    bool performFirmwareUpdate();
    bool performSDUpdate();

private:
    Config _cfg;
    String _latestVersion;
    bool _updateAvailable;
    unsigned long _lastCheckTime;

    String getAssetUrl(const char* assetName);
    void drawProgress(const String& title, int progress, int total);
    void extractTar(const char* tarPath, const char* destDir);
    void writeVersionFile(const String& version);
};
