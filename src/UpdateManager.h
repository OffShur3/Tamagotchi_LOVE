#ifndef UPDATE_MANAGER_H
#define UPDATE_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <SD_MMC.h>
#include <FS.h>

// Archivos
#include "Game.h"

#define GITHUB_USER "OffShur3"
#define GITHUB_REPO "Tamagotchi_LOVE"
#define CURRENT_VERSION_FILE "/version.txt"
#define UPDATE_CHECK_INTERVAL 300000  // 30 segundos para pruebas

extern bool updateAvailable;
extern String latestVersion;
extern bool updateInProgress;
extern bool mandatoryUpdate;
extern Game* game; 

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
void performFullUpdate();                  // Actualización completa (SD + OTA)
void drawProgressScreen(const String& title, int progress, int total, bool redrawAll = false);
bool needMandatoryUpdate();               // Verifica si es obligatorio actualizar

#endif
