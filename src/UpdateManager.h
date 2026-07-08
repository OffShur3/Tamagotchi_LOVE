#ifndef UPDATE_MANAGER_H
#define UPDATE_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <SD_MMC.h>
#include <FS.h>

#define GITHUB_USER "OffShur3"
#define GITHUB_REPO "Tamagotchi_LOVE"
#define CURRENT_VERSION_FILE "version.txt"
#define UPDATE_CHECK_INTERVAL 10000  // 10 segundos para pruebas

extern bool updateAvailable;
extern String latestVersion;
extern bool updateInProgress;

bool checkForUpdate();
void drawUpdateBadge();
bool isTouchingBadge(uint16_t x, uint16_t y);
void showUpdatePopup();
void showConfirmPopup();
void performFirmwareUpdate();
void performSDUpdate();
String getCurrentVersion();
void extractTar(const char* tarPath, const char* destDir);
void updateVersionFile();

#endif
