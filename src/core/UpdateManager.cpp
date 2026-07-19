// src/core/UpdateManager.cpp
#include "UpdateManager.h"
#include <WiFi.h>
#include <memory> 

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

bool checkDnsSafely(const char* host) {
    IPAddress ip;
    int dnsRetries = 0;
    while(WiFi.hostByName(host, ip) != 1 && dnsRetries < 20) {
        delay(500);
        dnsRetries++;
    }
    return dnsRetries < 20;
}

bool UpdateManager::checkForUpdate() {
    if (WiFi.status() != WL_CONNECTED) return false;

    if (!checkDnsSafely("api.github.com")) return false;

    int retries = 3;
    while (retries > 0) {
        std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
        client->setInsecure(); 

        HTTPClient http;
        http.setReuse(false);
        String url = "https://api.github.com/repos/" + String(_cfg.githubUser) + "/" + String(_cfg.githubRepo) + "/releases/latest";

        if (http.begin(*client, url)) {
            http.addHeader("User-Agent", "Tamagotchi-ESP32");
            int httpCode = http.GET();

            if (httpCode == 200) {
                String payload = http.getString();
                
                int tagPos = payload.indexOf("\"tag_name\":\"");
                if (tagPos != -1) {
                    tagPos += 12;
                    int tagEnd = payload.indexOf("\"", tagPos);
                    _latestVersion = payload.substring(tagPos, tagEnd);
                    
                    if (_latestVersion != getCurrentVersion() && _latestVersion.length() > 0) {
                        _updateAvailable = true;
                        http.end();
                        return true; 
                    }
                    http.end();
                    return false; 
                }
            } 
            http.end();
        }
        retries--;
        if (retries > 0) delay(1500); 
    }
    return false;
}

void UpdateManager::drawMessage(const String& msg) {
    _lastTitle = ""; 
    _cfg.gfx->fillScreen(0x18C3); // BGR Blue Rustico
    _cfg.gfx->setTextColor(0xFFFF);
    _cfg.gfx->setTextSize(1);
    int textX = (172 - (msg.length() * 6)) / 2;
    if (textX < 0) textX = 0;
    _cfg.gfx->setCursor(textX, 150);
    _cfg.gfx->print(msg);
}

// INYECCIÓN DE TU CÓDIGO GRÁFICO CON TEXTO PORCENTUAL:
void UpdateManager::drawProgress(const String& title, int progress, int total) {
    // Dibujo general estático una vez:
    if (title != _lastTitle) {
        _cfg.gfx->fillScreen(0x18C3);
        _cfg.gfx->setTextColor(0xFFFF);
        _cfg.gfx->setTextSize(1);
        int textX = (172 - (title.length() * 6)) / 2;
        if (textX < 0) textX = 0;
        _cfg.gfx->setCursor(textX, 100);
        _cfg.gfx->print(title);
        _lastTitle = title;
    }

    if (total > 0) {
        int percentage = (progress * 100) / total;

        // Base Blanca de la barra (borde / fill background)
        _cfg.gfx->fillRect(20, 130, 132, 20, 0x1042);
        _cfg.gfx->drawRect(20, 130, 132, 20, 0xFFFF);
        
        // Relleno Verde Tierras del Stardew/Retro 
        int fillW = (128 * percentage) / 100;
        if (fillW > 128) fillW = 128;
        _cfg.gfx->fillRect(22, 132, fillW, 16, 0x54A8); 

        // Mostrar el porcentaje exacto (al 142 Y como tu referencia original!)
        int percentX = (172 - (4 * 6)) / 2; // Para ubicar algo en centro "100%"
        _cfg.gfx->setTextColor(0xFFFF);
        _cfg.gfx->setTextSize(1);
        _cfg.gfx->setCursor(percentX + 2, 160);
        _cfg.gfx->printf("%d%%", percentage);

    } else {
        // En caso que haya animacion desconocida (Content Length: -1)
        _cfg.gfx->drawRect(20, 130, 132, 20, 0xFFFF);
        int fill = (progress % 102400) * 128 / 102400;
        _cfg.gfx->fillRect(22, 132, fill, 16, 0x54A8);
        _cfg.gfx->fillRect(22 + fill, 132, 128 - fill, 16, 0x18C3);
    }
}

String UpdateManager::getAssetUrl(const char* assetName) {
    if (_latestVersion.length() == 0 || _latestVersion == "") {
        checkForUpdate(); 
    }
    return "https://github.com/" + String(_cfg.githubUser) + "/" + String(_cfg.githubRepo) + "/releases/download/" + _latestVersion + "/" + String(assetName);
}

bool UpdateManager::performFullUpdate() {
    drawMessage("Estableciendo Carga...");
    delay(1000); 

    if (_latestVersion.length() == 0 || _latestVersion == "") {
        if (!checkForUpdate()) {
            drawMessage("Nube inaccesible");
            delay(3000);
            ESP.restart();
            return false;
        }
    }

    if (!performSDUpdate()) {
        drawMessage("Fallo Servidor SD");
        delay(3000);
        ESP.restart(); 
        return false;
    }

    drawMessage("Cerrando Base OS...");
    delay(1000);

    if (performFirmwareUpdate()) {
        writeVersionFile(_latestVersion);
        drawMessage("Reiniciando...");
        delay(2000);
        ESP.restart();
        return true;
    }
    
    drawMessage("ROM Corrupta!");
    delay(3000);
    ESP.restart();
    return false;
}

bool UpdateManager::performSDUpdate() {
    String url = getAssetUrl("sd_files.tar");
    drawMessage("Recopilando Entorno...");

    if (!checkDnsSafely("github.com")) return false;

    int retries = 0;
    while (retries < 10) {
        std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
        client->setInsecure();
        
        HTTPClient http;
        http.setReuse(false);
        const char* headerKeys[] = {"Location"};
        http.collectHeaders(headerKeys, 1);

        if (!http.begin(*client, url)) {
            delay(1000);
            retries++;
            continue;
        }

        int httpCode = http.GET();
        if (httpCode == 301 || httpCode == 302) {
            url = http.header("Location");
            http.end();
            continue; 
        }

        if (httpCode == 200) {
            int total = http.getSize();
            File file = SD_MMC.open("/update.tar", "w");
            if (!file) {
                http.end();
                return false;
            }

            WiFiClient* stream = http.getStreamPtr();
            std::unique_ptr<uint8_t[]> buffer(new uint8_t[4096]); 
            int written = 0;
            int len = total;
            int lastProgress = 0; 

            drawProgress("Bajando Recursos...", 0, total);

            while (http.connected() && (len > 0 || len == -1)) {
                size_t size = stream->available();
                if (size) {
                    size_t toRead = (size > 4096) ? 4096 : size;
                    if (len > 0 && toRead > len) toRead = len; 
                    
                    int c = stream->readBytes(buffer.get(), toRead);
                    file.write(buffer.get(), c);
                    written += c;
                    if (len > 0) len -= c;
                    
                    if (written - lastProgress > 25600) {
                        drawProgress("Bajando Recursos...", written, total);
                        lastProgress = written;
                    }
                } else {
                    delay(1); // LA MEDICINA REAL AL DESVANECIMIENTO ESP32
                }
            }
            
            drawProgress("Bajando Recursos...", total, total);
            file.close();
            http.end();

            drawProgress("Descomprimiendo SD", 50, 100); 
            extractTar("/update.tar", "/");
            SD_MMC.remove("/update.tar");
            return true;
        }

        if (httpCode == 404) {
            drawMessage(String("Cociendo nube.. ") + String(10 - retries));
            http.end();
            delay(8000);
            retries++;
        } else {
            drawMessage("Latencia Servidores...");
            http.end();
            delay(3000);
            retries++;
        }
    }
    return false;
}

bool UpdateManager::performFirmwareUpdate() {
    String url = getAssetUrl("firmware.bin");
    drawMessage("Estabilizando Kernel");

    if (!checkDnsSafely("github.com")) return false;

    int retries = 0;
    while (retries < 10) {
        std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
        client->setInsecure();
        
        HTTPClient http;
        http.setReuse(false);
        const char* headerKeys[] = {"Location"};
        http.collectHeaders(headerKeys, 1);

        if (!http.begin(*client, url)) {
            delay(1000);
            retries++;
            continue;
        }

        int httpCode = http.GET();
        if (httpCode == 301 || httpCode == 302) {
            url = http.header("Location");
            http.end();
            continue; 
        }

        if (httpCode == 200) {
            int contentLength = http.getSize();
            bool canBegin = (contentLength > 0) ? Update.begin(contentLength, U_FLASH) : Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);

            if (!canBegin) {
                http.end();
                return false;
            }

            WiFiClient* stream = http.getStreamPtr();
            std::unique_ptr<uint8_t[]> buffer(new uint8_t[4096]); 
            size_t written = 0;
            int len = contentLength;
            int lastProgress = 0; 

            drawProgress("Sobreescribiendo...", 0, contentLength);

            while (http.connected() && (len > 0 || len == -1)) {
                size_t size = stream->available();
                if (size) {
                    size_t toRead = (size > 4096) ? 4096 : size;
                    if (len > 0 && toRead > len) toRead = len;
                    
                    int c = stream->readBytes(buffer.get(), toRead);
                    size_t res = Update.write(buffer.get(), c);
                    
                    if (res != c) {
                        return false; 
                    }
                    
                    written += c;
                    if (len > 0) len -= c;
                    
                    if (written - lastProgress > 25600) { 
                        drawProgress("Sobreescribiendo...", written, contentLength);
                        lastProgress = written;
                    }
                } else {
                    delay(1); // LA MEDICINA DE RETRO-ALIMENTACIÓN WATCHDOG
                }
            }
            
            drawProgress("Sobreescribiendo...", contentLength, contentLength);

            if (written == contentLength) {
                if (Update.end(true)) {
                    writeVersionFile(_latestVersion);
                    drawMessage("Kernel Actualizado!");
                    Serial.flush();
                    delay(2000);
                    ESP.restart(); 
                    return true; // Ya sabemos que no llega ni se evalua de todos modos aquí :)
                }
            } 
            Update.end(false); 
            http.end();
            return false;
        }

        if (httpCode == 404) {
            drawMessage(String("Cociendo Nucleo ") + String(10 - retries));
            http.end();
            delay(8000);
            retries++;
        } else {
            drawMessage("Límites Nube T1");
            http.end();
            delay(3000);
            retries++;
        }
    }
    return false;
}

void UpdateManager::extractTar(const char* tarPath, const char* destDir) {
    File tarFile = SD_MMC.open(tarPath, "r");
    if (!tarFile) return;

    uint8_t buffer[512];
    int count = 0;

    while (tarFile.read(buffer, 512) == 512) {
        bool allZero = true;
        for (int i = 0; i < 512; i++) {
            if (buffer[i] != 0) { allZero = false; break; }
        }
        if (allZero) break; 

        char filename[101];
        memcpy(filename, buffer, 100);
        filename[100] = '\0';

        char typeflag = buffer[156];
        
        size_t fileSize = 0;
        for (int i = 0; i < 11; i++) {
            if (buffer[124 + i] >= '0' && buffer[124 + i] <= '7')
                fileSize = fileSize * 8 + (buffer[124 + i] - '0');
        }

        if (strlen(filename) == 0) continue;

        String fileStr = String(filename);
        if (fileStr.startsWith("./")) {
            fileStr = fileStr.substring(2); 
        }
        if (fileStr.length() == 0) continue; 

        String fullPath = String(destDir);
        if (!fullPath.endsWith("/")) fullPath += "/";
        fullPath += fileStr;

        if (typeflag == '5' || (fileSize == 0 && fullPath.endsWith("/"))) { 
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
                    int chunk = remaining > 512 ? 512 : remaining;
                    tarFile.read(buffer, chunk); 
                    outFile.write(buffer, chunk);
                    remaining -= chunk;
                }
                outFile.close();
                count++;
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
