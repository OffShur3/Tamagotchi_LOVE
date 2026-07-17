// src/core/UpdateManager.cpp
#include "UpdateManager.h"
#include <WiFi.h>

UpdateManager::UpdateManager(const Config& config)
    : _cfg(config), _latestVersion(""), _updateAvailable(false), _lastTitle("") {}

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
    if (!SD_MMC.exists("/QR Network.png") || getCurrentVersion() == "v0.0.0") {
        return true;
    }
    return _updateAvailable;
}

bool UpdateManager::checkForUpdate() {
    if (WiFi.status() != WL_CONNECTED) return false;

    int waitCount = 0;
    while (WiFi.localIP().toString() == "0.0.0.0" && waitCount < 50) {
        delay(100);
        waitCount++;
    }
    if (WiFi.localIP().toString() == "0.0.0.0") {
        Serial.println("[OTA] Error: Falló DHCP (No hay IP).");
        return false;
    }

    delay(1000); // Estabilización del DNS del router

    int retries = 3;
    while (retries > 0) {
        WiFiClientSecure client;
        client.setInsecure(); 
        client.setTimeout(10000); 

        HTTPClient http;
        http.setTimeout(10000);
        String url = "https://api.github.com/repos/" + String(_cfg.githubUser) + "/" + String(_cfg.githubRepo) + "/releases/latest";

        if (http.begin(client, url)) {
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
                    Serial.printf("[OTA] Versión actual: %s | Última en GitHub: %s\n", current.c_str(), _latestVersion.c_str());
                    
                    if (_latestVersion != current && _latestVersion.length() > 0) {
                        _updateAvailable = true;
                    }
                }
                http.end();
                client.stop();
                return _updateAvailable;
            } else {
                Serial.printf("[OTA] Error API GitHub: %s (Código: %d)\n", http.errorToString(httpCode).c_str(), httpCode);
            }
            http.end();
        } 
        client.stop();

        retries--;
        if (retries > 0) delay(2000);
    }
    return false;
}

void UpdateManager::drawMessage(const String& msg) {
    _lastTitle = ""; 
    _cfg.gfx->fillScreen(0x18C3);
    _cfg.gfx->setTextColor(0xFFFF);
    _cfg.gfx->setTextSize(1);
    int textX = (172 - (msg.length() * 6)) / 2;
    if (textX < 0) textX = 0;
    _cfg.gfx->setCursor(textX, 150);
    _cfg.gfx->print(msg);
}

void UpdateManager::drawProgress(const String& title, int progress, int total) {
    if (title != _lastTitle) {
        _cfg.gfx->fillScreen(0x18C3);
        _cfg.gfx->setTextColor(0xFFFF);
        _cfg.gfx->setTextSize(1);
        int textX = (172 - (title.length() * 6)) / 2;
        if (textX < 0) textX = 0;
        _cfg.gfx->setCursor(textX, 100);
        _cfg.gfx->print(title);
        _cfg.gfx->drawRect(20, 130, 132, 20, 0xFFFF);
        _lastTitle = title;
    }
    
    if (total > 0) {
        int fill = (128 * progress) / total;
        if (fill > 128) fill = 128;
        _cfg.gfx->fillRect(22, 132, fill, 16, 0x54A8); 
    } else {
        int fill = (progress % 102400) * 128 / 102400;
        _cfg.gfx->fillRect(22, 132, fill, 16, 0x54A8);
        _cfg.gfx->fillRect(22 + fill, 132, 128 - fill, 16, 0x18C3);
    }
}

String UpdateManager::getAssetUrl(const char* assetName) {
    return "https://github.com/" + String(_cfg.githubUser) + "/" + String(_cfg.githubRepo) + "/releases/download/" + _latestVersion + "/" + String(assetName);
}

bool UpdateManager::performFullUpdate() {
    drawMessage("Liberando RAM...");
    // 1. Ya se borró el juego en main.cpp. Aquí frenamos 1.5s para que LwIP 
    //    destruya toda la basura de red acumulada y libere los sockets 100%.
    delay(1500); 
    Serial.printf("[OTA] RAM Libre antes de iniciar: %u bytes\n", ESP.getFreeHeap());

    Serial.println("[OTA] Iniciando actualización de SD...");
    if (!performSDUpdate()) {
        Serial.println("[OTA] Error actualizando la SD.");
        drawMessage("Fallo al bajar la SD");
        delay(3000);
        ESP.restart(); 
    }

    drawMessage("Preparando firmware...");
    delay(1000); // Evitar saturación antes de la 2da descarga

    Serial.println("[OTA] Iniciando actualización de Firmware...");
    if (performFirmwareUpdate()) {
        writeVersionFile(_latestVersion);
        Serial.println("[OTA] ¡Actualización completada! Reiniciando...");
        drawMessage("Actualizacion Lista!");
        delay(2000);
        ESP.restart();
        return true;
    }
    
    Serial.println("[OTA] Falló la actualización del Firmware.");
    drawMessage("Fallo el Firmware");
    delay(3000);
    ESP.restart();
    return false;
}

bool UpdateManager::performSDUpdate() {
    String url = getAssetUrl("sd_files.tar");
    drawMessage("Buscando SD...");
    int retries = 0;

    // AHORA EL BUCLE REINTENTA INCLUSO SI SE CORTA EL INTERNET O FALLA EL DNS
    while (retries < 15) {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(10000);
        
        HTTPClient http;
        const char* headerKeys[] = {"Location"};
        http.collectHeaders(headerKeys, 1);

        if (!http.begin(client, url)) {
            client.stop();
            delay(2000);
            retries++;
            continue;
        }

        int httpCode = http.GET();

        // REDIRECCIÓN LIMPIA Y DESTRUCCIÓN DEL ENLACE VIEJO
        if (httpCode == 301 || httpCode == 302) {
            url = http.header("Location");
            Serial.println("[OTA] Redirección detectada. Saltando...");
            http.end();
            client.stop();
            continue; 
        }

        if (httpCode == 200) {
            int total = http.getSize();
            File file = SD_MMC.open("/update.tar", "w");
            if (!file) {
                http.end();
                client.stop();
                return false;
            }

            WiFiClient* stream = http.getStreamPtr();
            uint8_t buffer[1024];
            int written = 0;
            int len = total;

            drawProgress("Descargando assets...", 0, total);

            while (http.connected() && (len > 0 || len == -1)) {
                size_t size = stream->available();
                if (size) {
                    int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
                    file.write(buffer, c);
                    written += c;
                    if (len > 0) len -= c;
                    drawProgress("Descargando assets...", written, total);
                }
                delay(1);
            }
            file.close();
            http.end();
            client.stop();

            drawProgress("Extrayendo archivos...", 50, 100);
            extractTar("/update.tar", "/");
            SD_MMC.remove("/update.tar");
            return true; // ÉXITO
        }

        if (httpCode == 404) {
            drawMessage(String("Compilando... ") + String(15 - retries));
            http.end();
            client.stop();
            delay(10000);
            retries++;
        } else {
            // CUALQUIER OTRO ERROR (DNS, Timeout, etc): Ahora REINTENTA, no aborta
            Serial.printf("[OTA] Error SD HTTP: %s (%d)\n", http.errorToString(httpCode).c_str(), httpCode);
            drawMessage("Reintentando Red...");
            http.end();
            client.stop();
            delay(5000); // 5 segundos para que el router reviva
            retries++;
        }
    }
    return false;
}

bool UpdateManager::performFirmwareUpdate() {
    String url = getAssetUrl("firmware.bin");
    drawMessage("Buscando Firmware...");
    int retries = 0;

    while (retries < 15) {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(10000);
        
        HTTPClient http;
        const char* headerKeys[] = {"Location"};
        http.collectHeaders(headerKeys, 1);

        if (!http.begin(client, url)) {
            client.stop();
            delay(2000);
            retries++;
            continue;
        }

        int httpCode = http.GET();

        if (httpCode == 301 || httpCode == 302) {
            url = http.header("Location");
            Serial.println("[OTA] Redirección Firmware detectada...");
            http.end();
            client.stop();
            continue; 
        }

        if (httpCode == 200) {
            int contentLength = http.getSize();
            bool canBegin = (contentLength > 0) ? Update.begin(contentLength) : Update.begin(UPDATE_SIZE_UNKNOWN);

            if (!canBegin) {
                Serial.println("[OTA] Error iniciando motor Update.");
                http.end();
                client.stop();
                return false;
            }

            WiFiClient* stream = http.getStreamPtr();
            uint8_t buffer[1024];
            size_t written = 0;
            int len = contentLength;

            drawProgress("Flasheando TAMA...", 0, contentLength);

            while (http.connected() && (len > 0 || len == -1)) {
                size_t size = stream->available();
                if (size) {
                    int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
                    Update.write(buffer, c);
                    written += c;
                    if (len > 0) len -= c;
                    drawProgress("Flasheando TAMA...", written, contentLength);
                }
                delay(1);
            }

            bool success = Update.end(true);
            http.end();
            client.stop();
            return success;
        }

        if (httpCode == 404) {
            drawMessage(String("Firmware... ") + String(15 - retries));
            http.end();
            client.stop();
            delay(10000);
            retries++;
        } else {
            // REINTENTO EN CORTES DE LUZ O RED
            Serial.printf("[OTA] Error FW HTTP: %s (%d)\n", http.errorToString(httpCode).c_str(), httpCode);
            drawMessage("Reintentando Red...");
            http.end();
            client.stop();
            delay(5000);
            retries++;
        }
    }
    return false;
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
                continue; 
            }
            
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
