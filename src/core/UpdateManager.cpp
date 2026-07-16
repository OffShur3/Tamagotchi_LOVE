// src/core/UpdateManager.cpp
#include "UpdateManager.h"
#include <WiFi.h> // SOLUCIONA: WiFi, WL_CONNECTED y WiFiClient no declarados

UpdateManager::UpdateManager(const Config& config)
    : _cfg(config), _latestVersion(""), _updateAvailable(false), _lastCheckTime(0) {}

String UpdateManager::getCurrentVersion() {
    if (!SD_MMC.exists(_cfg.currentVersionPath)) {
        return "v0.0.0";
    }
    File file = SD_MMC.open(_cfg.currentVersionPath, "r");
    if (!file) return "v0.0.0";
    String version = file.readStringUntil('\n');
    version.trim();
    file.close();
    return version;
}

bool UpdateManager::checkForUpdate() {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.setTimeout(10000);
    String url = "https://api.github.com/repos/" + String(_cfg.githubUser) + "/" + String(_cfg.githubRepo) + "/releases/latest";

    http.begin(url);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");
    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        int tagPos = payload.indexOf("\"tag_name\":\"");
        if (tagPos != -1) {
            tagPos += 12;
            int tagEnd = payload.indexOf("\"", tagPos);
            _latestVersion = payload.substring(tagPos, tagEnd);
            
            String current = getCurrentVersion();
            if (_latestVersion != current && _latestVersion.length() > 0) {
                _updateAvailable = true;
                http.end();
                return true;
            }
        }
    }
    http.end();
    return false;
}

bool UpdateManager::needMandatoryUpdate() {
    if (!SD_MMC.exists("/QR Network.png") || getCurrentVersion() == "v0.0.0") {
        return true;
    }
    return _updateAvailable;
}

void UpdateManager::drawUpdateBadge(int x, int y, int radius) {
    _cfg.gfx->fillCircle(x, y, radius, 0xF800); 
    _cfg.gfx->setTextColor(0xFFFF);            
    _cfg.gfx->setTextSize(1);
    _cfg.gfx->setCursor(x - 3, y - 4);
    _cfg.gfx->print("!");
}

bool UpdateManager::isTouchingBadge(uint16_t touchX, uint16_t touchY, int badgeX, int badgeY, int radius) {
    int dx = (int)touchX - badgeX;
    int dy = (int)touchY - badgeY;
    return (dx * dx + dy * dy) <= (radius * radius);
}

String UpdateManager::getAssetUrl(const char* assetName) {
    if (WiFi.status() != WL_CONNECTED) return "";

    HTTPClient http;
    String url = "https://api.github.com/repos/" + String(_cfg.githubUser) + "/" + String(_cfg.githubRepo) + "/releases/latest";
    http.begin(url);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");
    int httpCode = http.GET();

    String downloadUrl = "";
    if (httpCode == 200) {
        String payload = http.getString();
        int assetPos = payload.indexOf(assetName);
        if (assetPos != -1) {
            int urlPos = payload.indexOf("\"browser_download_url\":\"", assetPos);
            if (urlPos != -1) {
                urlPos += 24;
                int urlEnd = payload.indexOf("\"", urlPos);
                downloadUrl = payload.substring(urlPos, urlEnd);
                downloadUrl.replace("\\/", "/"); 
            }
        }
    }
    http.end();
    return downloadUrl;
}

void UpdateManager::drawProgress(const String& title, int progress, int total) {
    _cfg.gfx->fillScreen(0x18C3);
    _cfg.gfx->setCursor(20, 80);
    _cfg.gfx->setTextColor(0xFFFF);
    _cfg.gfx->setTextSize(1);
    _cfg.gfx->print(title);
    
    int barW = 132;
    _cfg.gfx->drawRect(20, 130, barW, 20, 0xFFFF);
    if (total > 0) {
        int fill = (barW - 4) * progress / total;
        _cfg.gfx->fillRect(22, 132, fill, 16, 0x07E0); 
    }
}

bool UpdateManager::performFullUpdate() {
    if (performSDUpdate() && performFirmwareUpdate()) {
        writeVersionFile(_latestVersion);
        Serial.println("[OTA] Actualización completa terminada. Reiniciando...");
        delay(1000);
        ESP.restart();
        return true;
    }
    return false;
}

bool UpdateManager::performFirmwareUpdate() {
    String fwUrl = getAssetUrl("firmware.bin");
    if (fwUrl.length() == 0) return false;

    HTTPClient http;
    http.begin(fwUrl);
    int httpCode = http.GET();
    if (httpCode != 200) {
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        http.end();
        return false;
    }

    if (!Update.begin(contentLength)) {
        http.end();
        return false;
    }

    WiFiClient* client = http.getStreamPtr();
    size_t written = Update.writeStream(*client);

    if (written == contentLength) {
        if (Update.end(true)) {
            Serial.println("[OTA] Firmware grabado con éxito.");
            http.end();
            return true;
        }
    }
    Update.end(false);
    http.end();
    return false;
}

bool UpdateManager::performSDUpdate() {
    String sdUrl = getAssetUrl("sd_files.tar");
    if (sdUrl.length() == 0) return false;

    HTTPClient http;
    http.begin(sdUrl);
    int httpCode = http.GET();
    if (httpCode != 200) {
        http.end();
        return false;
    }

    File file = SD_MMC.open("/sd_files.tar", "w");
    if (!file) {
        http.end();
        return false;
    }

    int total = http.getSize();
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[512];
    int written = 0;

    while (http.connected() && (written < total)) {
        size_t size = stream->available();
        if (size) {
            int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
            file.write(buffer, c);
            written += c;
            drawProgress("Descargando SD...", written, total);
        }
        delay(1);
    }
    file.close();
    http.end();

    drawProgress("Extrayendo SD...", 50, 100);
    extractTar("/sd_files.tar", "/");
    
    SD_MMC.remove("/sd_files.tar");
    return true;
}

void UpdateManager::extractTar(const char* tarPath, const char* destDir) {
    File tarFile = SD_MMC.open(tarPath, "r");
    if (!tarFile) return;

    uint8_t buffer[512];
    while (tarFile.read(buffer, 512) == 512) {
        bool allZero = true;
        for (int i = 0; i < 512; i++) {
            if (buffer[i] != 0) { allZero = false; break; }
        }
        if (allZero) break; 

        char filename[256];
        memset(filename, 0, sizeof(filename));
        strncpy(filename, (char*)buffer, 100);

        char typeflag = buffer[156];
        
        char sizeStr[12];
        memcpy(sizeStr, &buffer[124], 11);
        sizeStr[11] = '\0';
        long fileSize = strtol(sizeStr, nullptr, 8);

        if (filename[0] == '\0') continue;

        String fullPath = String(destDir) + "/" + String(filename);

        if (typeflag == '5') { 
            SD_MMC.mkdir(fullPath.c_str());
        } else { 
            int lastSlash = fullPath.lastIndexOf('/');
            if (lastSlash != -1) {
                SD_MMC.mkdir(fullPath.substring(0, lastSlash).c_str());
            }

            File outFile = SD_MMC.open(fullPath.c_str(), "w");
            if (outFile) {
                long remaining = fileSize;
                while (remaining > 0) {
                    int toRead = remaining > 512 ? 512 : remaining;
                    int bytesRead = tarFile.read(buffer, 512);
                    if (bytesRead <= 0) break;
                    outFile.write(buffer, toRead);
                    remaining -= toRead;
                }
                outFile.close();
            } else {
                long blocksToSkip = (fileSize + 511) / 512;
                tarFile.seek(tarFile.position() + blocksToSkip * 512);
            }
        }
    }
    tarFile.close();
}

void UpdateManager::writeVersionFile(const String& version) {
    File file = SD_MMC.open(_cfg.currentVersionPath, "w");
    if (file) {
        file.println(version);
        file.close();
    }
}
