// src/core/UpdateManager.cpp
#include "UpdateManager.h"
#include <WiFi.h>

UpdateManager::UpdateManager(const Config& config)
    : _cfg(config), _latestVersion(""), _updateAvailable(false) {}

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

bool UpdateManager::needMandatoryUpdate() {
    // Forzar actualización si falta un archivo crítico, si no hay versión instalada, o si hay actualización disponible
    if (!SD_MMC.exists("/QR Network.png") || getCurrentVersion() == "v0.0.0") {
        return true;
    }
    return _updateAvailable;
}

bool UpdateManager::checkForUpdate() {
    if (WiFi.status() != WL_CONNECTED) return false;

    // CRÍTICO: Cliente seguro ignorando certificados para no agotar la RAM
    WiFiClientSecure client;
    client.setInsecure(); 

    HTTPClient http;
    http.setTimeout(10000);
    String url = "https://api.github.com/repos/" + String(_cfg.githubUser) + "/" + String(_cfg.githubRepo) + "/releases/latest";

    http.begin(client, url);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");
    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        
        // Parseo ligero (Zero-Allocation) buscando el tag para ahorrar RAM
        int tagPos = payload.indexOf("\"tag_name\":\"");
        if (tagPos != -1) {
            tagPos += 12;
            int tagEnd = payload.indexOf("\"", tagPos);
            _latestVersion = payload.substring(tagPos, tagEnd);
            
            String current = getCurrentVersion();
            Serial.printf("[OTA] Versión actual: %s | Última en GitHub: %s\n", current.c_str(), _latestVersion.c_str());
            
            if (_latestVersion != current && _latestVersion.length() > 0) {
                _updateAvailable = true;
                http.end();
                return true;
            }
        }
    } else {
        Serial.printf("[OTA] Error consultando API de GitHub. HTTP Code: %d\n", httpCode);
    }
    http.end();
    return false;
}

String UpdateManager::getAssetUrl(const char* assetName) {
    if (WiFi.status() != WL_CONNECTED) return "";

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = "https://api.github.com/repos/" + String(_cfg.githubUser) + "/" + String(_cfg.githubRepo) + "/releases/latest";
    http.begin(client, url);
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
                downloadUrl.replace("\\/", "/"); // Limpiar escapes del JSON
            }
        }
    }
    http.end();
    return downloadUrl;
}

void UpdateManager::drawProgress(const String& title, int progress, int total) {
    _cfg.gfx->fillScreen(0x18C3);
    _cfg.gfx->setTextColor(0xFFFF);
    _cfg.gfx->setTextSize(1);
    
    int textX = (172 - (title.length() * 6)) / 2;
    _cfg.gfx->setCursor(textX, 100);
    _cfg.gfx->print(title);
    
    int barW = 132;
    _cfg.gfx->drawRect(20, 130, barW, 20, 0xFFFF);
    if (total > 0) {
        int fill = (barW - 4) * progress / total;
        _cfg.gfx->fillRect(22, 132, fill, 16, 0x07E0);
    }
}

bool UpdateManager::performFullUpdate() {
    Serial.println("[OTA] Iniciando actualización de SD...");
    performSDUpdate(); 

    Serial.println("[OTA] Iniciando actualización de Firmware...");
    if (performFirmwareUpdate()) {
        writeVersionFile(_latestVersion);
        Serial.println("[OTA] ¡Actualización completada! Reiniciando...");
        delay(1000);
        ESP.restart();
        return true;
    }
    
    Serial.println("[OTA] Falló la actualización.");
    return false;
}

bool UpdateManager::performFirmwareUpdate() {
    String fwUrl = getAssetUrl("firmware.bin");
    if (fwUrl.length() == 0) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, fwUrl);
    
    // CRÍTICO para GitHub Releases: Seguir la redirección a los servidores S3 de AWS
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[OTA] Error descargando firmware.bin. Código HTTP: %d\n", httpCode);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        http.end();
        return false;
    }

    if (!Update.begin(contentLength)) {
        Serial.println("[OTA] Error iniciando motor de Update en ESP32.");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buffer[1024];

    while (http.connected() && (written < contentLength)) {
        size_t size = stream->available();
        if (size) {
            int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
            Update.write(buffer, c);
            written += c;
            drawProgress("Flasheando TAMA...", written, contentLength);
        }
        delay(1);
    }

    bool success = false;
    if (written == contentLength) {
        success = Update.end(true);
    } else {
        Update.end(false);
    }

    http.end();
    return success;
}

bool UpdateManager::performSDUpdate() {
    String sdUrl = getAssetUrl("sd_files.tar");
    if (sdUrl.length() == 0) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, sdUrl);
    
    // CRÍTICO para GitHub Releases
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = http.GET();
    if (httpCode != 200) {
        http.end();
        return false;
    }

    File file = SD_MMC.open("/update.tar", "w");
    if (!file) {
        http.end();
        return false;
    }

    int total = http.getSize();
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    int written = 0;

    while (http.connected() && (written < total)) {
        size_t size = stream->available();
        if (size) {
            int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
            file.write(buffer, c);
            written += c;
            drawProgress("Descargando assets...", written, total);
        }
        delay(1);
    }
    file.close();
    http.end();

    drawProgress("Extrayendo archivos...", 50, 100);
    extractTar("/update.tar", "/");
    SD_MMC.remove("/update.tar");
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
        if (allZero) break; // Fin de archivo TAR detectado (bloque vacío)

        char filename[101];
        memset(filename, 0, sizeof(filename));
        strncpy(filename, (char*)buffer, 100);

        char typeflag = buffer[156];
        
        char sizeStr[12];
        memcpy(sizeStr, &buffer[124], 11);
        sizeStr[11] = '\0';
        long fileSize = strtol(sizeStr, nullptr, 8);

        if (filename[0] == '\0') continue;

        String fullPath = String(destDir);
        if (!fullPath.endsWith("/")) fullPath += "/";
        fullPath += String(filename);

        if (typeflag == '5') { // Es un directorio
            SD_MMC.mkdir(fullPath.c_str());
        } else { // Es un archivo
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
                // Saltar el archivo en la lectura del TAR si no se pudo escribir
                long blocksToSkip = (fileSize + 511) / 512;
                tarFile.seek(tarFile.position() + blocksToSkip * 512);
                continue; 
            }
            
            // Los archivos dentro de un TAR siempre tienen un relleno (padding) de nulos
            // hasta completar un bloque exacto de 512 bytes. Esto limpia ese sobrante.
            long padding = (512 - (fileSize % 512)) % 512;
            if (padding > 0) {
                tarFile.seek(tarFile.position() + padding);
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
