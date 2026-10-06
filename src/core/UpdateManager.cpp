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
    while(WiFi.hostByName(host, ip) != 1 && dnsRetries < 15) {
        Serial.printf("[OTA] DNS: Esperando Nodos para la traduccion de Direccion IP Server.. Intento %d...\n", dnsRetries);
        delay(500);
        dnsRetries++;
    }
    return dnsRetries < 15;
}

bool UpdateManager::checkForUpdate() {
    if (WiFi.status() != WL_CONNECTED) return false;
    if (!checkDnsSafely("api.github.com")) return false;

    int retries = 3;
    while (retries > 0) {
        std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
        client->setInsecure(); 

        HTTPClient http;
        http.setTimeout(15000); // 15s timeout global (elimina el Bad File Number)
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

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
                    
                    String current = getCurrentVersion();
                    Serial.printf("[OTA] Firmware Original en Operación: %s | Último Lote de Master Branch Carga TAMA %s\n", current.c_str(), _latestVersion.c_str());
                    
                    if (_latestVersion != current && _latestVersion.length() > 0) {
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
        if (retries > 0) delay(1000); 
    }
    return false;
}

void UpdateManager::drawMessage(const String& msg) {
    _lastTitle = ""; 
    
    // Paleta Stardew/Handheld Retro Cute
    uint16_t cBg     = 0xF6FA; // Crema cálido
    uint16_t cBorder = 0x4903; // Marrón oscuro
    uint16_t cGold   = 0xCE40; // Toque decorativo interior dorado

    _cfg.gfx->fillScreen(cBg); 
    
    // Doble marco para simular "cartelito del tamagotchi"
    _cfg.gfx->drawRect(4, 4, 164, 312, cBorder);
    _cfg.gfx->drawRect(6, 6, 160, 308, cBorder);
    _cfg.gfx->drawRect(5, 5, 162, 310, cGold);

    // Antenita decorativa arriba simulando "Wifi en proceso"
    _cfg.gfx->setTextColor(cBorder, cBg);
    _cfg.gfx->setTextSize(1);
    _cfg.gfx->setCursor((172 - (11 * 6)) / 2, 28);
    _cfg.gfx->print("<<< O_O >>>"); 

    // Mensaje Central Principal
    int textX = (172 - (msg.length() * 6)) / 2;
    if (textX < 0) textX = 0;
    _cfg.gfx->setCursor(textX, 150);
    _cfg.gfx->print(msg);
}

void UpdateManager::drawProgress(const String& title, int progress, int total) {
    // Definimos nuevamente la paleta cute para el dibujado incremental
    uint16_t cBg     = 0xF6FA; // Crema
    uint16_t cBorder = 0x4903; // Marrón
    uint16_t cGold   = 0xCE40; // Oro
    uint16_t cFill   = 0x54A8; // Verde naturaleza / éxito Stardew
    uint16_t cBarBg  = 0xFFFF; // Interior blanco puro de la barrita para altísimo contraste

    if (title != _lastTitle) {
        _cfg.gfx->fillScreen(cBg);
        
        _cfg.gfx->drawRect(4, 4, 164, 312, cBorder);
        _cfg.gfx->drawRect(6, 6, 160, 308, cBorder);
        _cfg.gfx->drawRect(5, 5, 162, 310, cGold);
        
        _cfg.gfx->setTextColor(cBorder, cBg);
        _cfg.gfx->setTextSize(1);
        _cfg.gfx->setCursor((172 - (11 * 6)) / 2, 28);
        _cfg.gfx->print("<<< v_v >>>"); // Cambio de antenita mientras ya bajando data
        
        int textX = (172 - (title.length() * 6)) / 2;
        if (textX < 0) textX = 0;
        _cfg.gfx->setCursor(textX, 90); // Arriba de la barra de descarga
        _cfg.gfx->print(title);
        _lastTitle = title;
    }

    // Config de nuestra Barra Chunky 2.0  (Un lindo ancho conteniendo toda la barra de relleno adentro)
    int barX = 16;
    int barY = 120;
    int barW = 140;
    int barH = 22;
    
    // Contorno estático marrón exterior (Sólido como hierro/maderita)
    _cfg.gfx->drawRect(barX, barY, barW, barH, cBorder);

    if (total > 0) {
        int percentage = (progress * 100) / total;
        
        int innerMaxW = barW - 4; // Margen pixelizado interno - evita rozar los marcos 
        int fillW = (innerMaxW * percentage) / 100;
        
        // Safety lockers
        if (fillW > innerMaxW) fillW = innerMaxW;
        if (fillW < 0) fillW = 0;

        // --- ACA ESTA EL TRUCO: DIBUJAMOS FILL EN VERDE... LUEGO TODO LO QUE QUEDA DENTRO LO BARRIDO VACIAMOS A BLANCO
        if(fillW > 0) {
            _cfg.gfx->fillRect(barX + 2, barY + 2, fillW, barH - 4, cFill);
        }
        if(fillW < innerMaxW) {
            _cfg.gfx->fillRect(barX + 2 + fillW, barY + 2, innerMaxW - fillW, barH - 4, cBarBg);
        }

        // Contador Numérico de Doble Parametro (Con Fondo Para no Solapar/Arruinar textos a cuadrados)
        _cfg.gfx->setTextColor(cBorder, cBg); // !! AQUI - Usamos la magia de render_string cBackground  !! 
        _cfg.gfx->setTextSize(2);             // Mas Grande el Número Porcentual!
        
        char buf[8];
        snprintf(buf, sizeof(buf), "%3d%%", percentage); // '%3d%%' rellena con un espacio blanco fijo números de un caracter protegiendo tamaño pixel! 
        int percentX = (172 - (4 * 12)) / 2;  // Size 2 ocupa en pixeles por default 12 de ancho
        
        _cfg.gfx->setCursor(percentX, 155);
        _cfg.gfx->print(buf);

    } else {
        // MODO BOLA DE NIEVE ROTATORIO/DESLIZANTE PARA CUALQUIER OTRA DESCARGA DESCONOCIDA (< 0 TAMANHO).
        int innerMaxW = barW - 4; 
        int fillW = (progress % 102400) * innerMaxW / 102400; // Oscilando

        if (fillW > innerMaxW) fillW = innerMaxW;
        if (fillW < 0) fillW = 0;

        if(fillW > 0) {
            _cfg.gfx->fillRect(barX + 2, barY + 2, fillW, barH - 4, cFill);
        }
        if(fillW < innerMaxW) {
            _cfg.gfx->fillRect(barX + 2 + fillW, barY + 2, innerMaxW - fillW, barH - 4, cBarBg);
        }
    }
}

String UpdateManager::getAssetUrl(const char* assetName) {
    if (_latestVersion.length() == 0 || _latestVersion == "") {
        Serial.println("[DEBUG-OTA] Limpio Version Caché Internamente C++. Obteniendo Puntero AWS en directo...");
        checkForUpdate(); 
    }
    return "https://github.com/" + String(_cfg.githubUser) + "/" + String(_cfg.githubRepo) + "/releases/download/" + _latestVersion + "/" + String(assetName);
}

bool UpdateManager::performFullUpdate() {
    Serial.println("\n=============================================");
    Serial.println("[DEBUG-OTA]      FULL TAMA OS NATIVE MANAGER ");
    Serial.println("=============================================\n");
    drawMessage("Mirando hacia el cielo...");
    delay(1000); 

    if (_latestVersion.length() == 0 || _latestVersion == "") {
        if (!checkForUpdate()) {
            drawMessage("El cielo no contesta :(");
            delay(3000);
            ESP.restart();
            return false;
        }
    }

    if (!performSDUpdate()) {
        drawMessage("Tropiezo en la red!");
        delay(3000);
        ESP.restart(); 
        return false;
    }

    drawMessage("Nuevos dibujos guardados.");
    delay(1000);

    if (performFirmwareUpdate()) {
        Serial.println("\n[DEBUG-OTA] Tarea Explicita Completa al 100%");
        writeVersionFile(_latestVersion);
        drawMessage("Hora de desperezar...");
        Serial.flush();
        delay(2500);
        ESP.restart();
        return true;
    }
    
    drawMessage("Oops, me confundi... :(");
    delay(4000);
    ESP.restart();
    return false;
}

bool UpdateManager::performSDUpdate() {
    String url = getAssetUrl("sd_files.tar");
    if (!checkDnsSafely("github.com")) return false;

    int retries = 3; 
    while (retries > 0) {
        Serial.printf("[DEBUG-OTA] Transferencia F1 Nube Server AWS Invocado [%d Tries]\n", retries);
        
        std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
        client->setInsecure(); 
        
        HTTPClient http;
        http.setTimeout(15000); // 15s timeout global
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 
        
        if (http.begin(*client, url)) {
            int httpCode = http.GET();
            
            if (httpCode == 200) {
                int total = http.getSize();
                File file = SD_MMC.open("/update.tar", "w");
                if (!file) {
                    http.end();
                    return false;
                }

                WiFiClient* stream = http.getStreamPtr();
                stream->setTimeout(15000); // Tolerancia vital a paquetes fragmentados
                
                // Forzamos el buffer temporal de 4KB a PSRAM para proteger el Kernel
                std::unique_ptr<uint8_t, decltype(&free)> buffer((uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM), &free);
                
                int written = 0;
                int len = total;
                int lastDraw = 0;
                bool wasCompleted = false;

                drawProgress("Bajando sorpresas...", 0, total);

                while (http.connected() && (len == -1 || written < len)) {
                    size_t available = stream->available();
                    if (available) {
                        size_t toRead = (available > 4096) ? 4096 : available;
                        int c = stream->readBytes(buffer.get(), toRead);
                        file.write(buffer.get(), c);
                        written += c;
                        
                        if (written - lastDraw >= 85400) { 
                             drawProgress("Llenando mi mochilita...", written, total);
                             lastDraw = written;
                             Serial.printf("[DEBUG-OTA] Alquilado: %d KB de su Server Activo!\n", written / 1024);
                        }
                    } else {
                        delay(1); 
                    }
                }
                
                file.close();

                if (written == total || (total == -1 && written > 0)) {
                    drawProgress("¡Listo atrapados!", 100, 100);
                    Serial.println("[DEBUG-OTA] Descargando de File F1 Estricto sin perdidas Completas Ok!");
                    wasCompleted = true;
                }

                http.end(); 
                
                if (wasCompleted) {
                    drawProgress("Abriendo bolsitas..", 50, 100);
                    extractTar("/update.tar", "/");
                    SD_MMC.remove("/update.tar");
                    return true;
                } 

                SD_MMC.remove("/update.tar");
            } 
            else if (httpCode == 404) {
                drawMessage("El cielo anda muy lento");
                http.end();
                delay(7000); 
            } else {
                http.end();
                delay(2000);
            }
        }
        retries--;
    }
    return false;
}

// EL NÚCLEO NATIVO MÁXIMO E INDESTRUCTIBLE PROTEGIDO CON TIMEOUT
bool UpdateManager::performFirmwareUpdate() {
    String url = getAssetUrl("firmware.bin");
    if (!checkDnsSafely("github.com")) return false;

    int retries = 3;
    while (retries > 0) {
        Serial.printf("\n[DEBUG-OTA] Ejecutando ROM Nucleo C++. AWS Interconnect... [%d Tries]\n", retries);
        
        std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
        client->setInsecure(); 
        
        HTTPClient http;
        http.setTimeout(15000); // 15s timeout global de conexión 
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        if (http.begin(*client, url)) {
            int httpCode = http.GET();
            
            if (httpCode == 200) {
                int totalLen = http.getSize();
                if (totalLen <= 0) totalLen = UPDATE_SIZE_UNKNOWN;

                bool canBegin = Update.begin(totalLen, U_FLASH);
                if (!canBegin) {
                    Serial.println("[DEBUG-OTA] Modulo Memoria Física OS Tildado. Falla Update API ESP32 begin().");
                    Update.printError(Serial);
                    http.end();
                    return false;
                }

                Update.onProgress([this](size_t progress, size_t total) {
                    static size_t lastProgress = 0;
                    if (progress - lastProgress > 95000 || progress == total) { 
                        int percent = (total > 0) ? (progress * 100) / total : 0;
                        Serial.printf("[DEBUG-OTA] Extinguiendo vieja base / Grabando ROM Nucleos... [ %d %% ]\n", percent);
                        this->drawProgress("Estudiando muchísimo..", progress, total);
                        lastProgress = progress;
                    }
                });

                WiFiClient* stream = http.getStreamPtr();
                stream->setTimeout(15000); // 15s timeout crítico para paquetes de red (Evita el corte de writeStream)
                
                Serial.println("[DEBUG-OTA] Instando Flujo OTA Nativo ESP (Blindado contra latencias de Amazon) ...");
                drawProgress("Leyendo mi cuentito.", 0, totalLen);

                // Volvemos a la rutina nativa del ESP32 protegida por nuestro setTimeout
                size_t writtenBytesCoreEsp = Update.writeStream(*stream);
                
                Update.onProgress(nullptr); 

                Serial.println("\n[DEBUG-OTA] Flujo nativo WriteStream concluido y clausurado limpiando Modems RAM...");
                Serial.printf("[DEBUG-OTA] Logrado Finalizar -> Escrito Pila ESP OS : %d / %d AWS Sizes!\n", writtenBytesCoreEsp, totalLen);

                if (writtenBytesCoreEsp > 0 && (totalLen == UPDATE_SIZE_UNKNOWN || writtenBytesCoreEsp == (size_t)totalLen)) {
                    Serial.println("[DEBUG-OTA] Calculando suma matematica del archivo con Encriptaciones C...");
                    
                    if (Update.end(true)) {
                        Serial.println("[DEBUG-OTA] => Veredicto NUBE ROM MD5... SUPERADO EXTREMO! FIRMWARE REESCRITO ESTRECHAMIENTO VALIDO. <==");
                        http.end();
                        return true;
                    } else {
                        Serial.print("[DEBUG-OTA] OUCH: VALIDACION EXPRESA EN CHIPSET MURIO MD5 CORROMPID!. RAZÓN FIRM: ");
                        Update.printError(Serial); 
                        Serial.println(); 
                    }
                } else {
                    Update.end(false); // EVADE LA ROTURA OS EN ARRANQUE CAIDA POR ERROR AWS TCP
                    Serial.print("[DEBUG-OTA] Despejo ROM corruptas y pedazos muertos que Cayeron AWS Redes Tapon ! Detalle ESP Nativo Core dice = ");
                    Update.printError(Serial);
                }

                http.end();
            } else if (httpCode == 404) {
                drawMessage("Me distraje con una flor.");
                http.end();
                delay(7000); 
            } else {
                http.end();
                delay(2000); 
            }
        }
        retries--;
    }
    return false;
}

void UpdateManager::extractTar(const char* tarPath, const char* destDir) {
    File tarFile = SD_MMC.open(tarPath, "r");
    if (!tarFile) {
        Serial.println("[TAR] ESP_FAIL. File Imposible Mapearlo y localizar Muestra !");
        return;
    }

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
                Serial.printf("[DEBUG-OTA] Compilado Estatico Integrado y Abrochado Local -> : %s\n", fullPath.c_str());
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
    Serial.printf("\n[DEBUG-OTA] Limpieza De Carpetas Comprimidas Terminó. Despliege Local 100%%: %d Directorios/File\n", count);
}

void UpdateManager::writeVersionFile(const String& version) {
    SD_MMC.mkdir("/config");
    File file = SD_MMC.open(_cfg.currentVersionPath, "w");
    if (file) {
        file.println(version);
        file.close();
    }
}
