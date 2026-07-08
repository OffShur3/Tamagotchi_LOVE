#include "UpdateManager.h"
#include "NetworkManager.h"
#include "colors.h"
#include "UI.h"
#include <ArduinoJson.h>

bool updateAvailable = false;
String latestVersion = "";
bool updateInProgress = false;

// --------------- Estructura para parsear TAR ---------------
struct TarHeader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
};

// --------------- Verificar actualización desde GitHub ---------------
bool checkForUpdate() {
    Serial.println("[UPDATE] checkForUpdate() llamada");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[UPDATE] No hay WiFi, saliendo");
        return false;
    }

    HTTPClient http;
    http.setTimeout(10000);
    String url = "https://api.github.com/repos/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) + "/releases/latest";
    Serial.printf("[UPDATE] Consultando: %s\n", url.c_str());

    http.begin(url);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");

    int httpCode = http.GET();
    Serial.printf("[UPDATE] HTTP code: %d\n", httpCode);

    if (httpCode != 200) {
        Serial.println("[UPDATE] Error HTTP, saliendo");
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();
    Serial.printf("[UPDATE] Respuesta (%d bytes): %s\n", payload.length(), payload.substring(0, 200).c_str());

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.printf("[UPDATE] Error JSON: %s\n", error.c_str());
        return false;
    }

    latestVersion = doc["tag_name"].as<String>();
    Serial.printf("[UPDATE] Latest version en GitHub: %s\n", latestVersion.c_str());

    String current = getCurrentVersion();
    Serial.printf("[UPDATE] Version actual en SD: %s\n", current.c_str());

    if (latestVersion != current && latestVersion.length() > 0) {
        Serial.println("[UPDATE] *** NUEVA VERSIÓN DISPONIBLE ***");
        updateAvailable = true;
        return true;
    }

    Serial.println("[UPDATE] Ya tienes la última versión");
    return false;
}

// --------------- Leer versión actual ---------------
String getCurrentVersion() {
    Serial.println("[UPDATE] Leyendo versión actual de la SD...");

    if (!SD_MMC.exists(CURRENT_VERSION_FILE)) {
        Serial.println("[UPDATE] /version.txt no existe, devolviendo v0.0.0");
        return "v0.0.0";
    }

    File file = SD_MMC.open(CURRENT_VERSION_FILE, "r");
    if (!file) {
        Serial.println("[UPDATE] Error al abrir /version.txt");
        return "v0.0.0";
    }

    String version = file.readStringUntil('\n');
    version.trim();
    file.close();
    Serial.printf("[UPDATE] Leído: '%s'\n", version.c_str());
    return version;
}

// --------------- Dibujar badge ---------------
void drawUpdateBadge() {
    int badgeX = 8, badgeY = 8, badgeR = 12;
    gfx->fillCircle(badgeX + badgeR, badgeY + badgeR, badgeR, BGR_RED);
    gfx->setTextColor(BGR_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(badgeX + 7, badgeY + 4);
    gfx->print("!");
}

bool isTouchingBadge(uint16_t x, uint16_t y) {
    bool tocando = (x >= 8 && x <= 32 && y >= 8 && y <= 32);
    if (tocando) {
        Serial.printf("[UPDATE] ¡Badge tocado! x=%d, y=%d\n", x, y);
    }
    return tocando;
}

// --------------- Popup de actualización ---------------
void showUpdatePopup() {
    Serial.println("[UPDATE] Mostrando popup de nueva versión");

    gfx->fillRoundRect(10, 80, 152, 160, 12, MAT_BG);
    gfx->drawRoundRect(10, 80, 152, 160, 12, BGR_WHITE);

    gfx->setTextColor(BGR_WHITE);
    gfx->setTextSize(1);
    imprimirCentrado("Nueva version!", 95, 1);

    gfx->setTextColor(BGR_GREEN);
    gfx->setTextSize(2);
    String versionText = latestVersion;
    int xPos = 86 - (versionText.length() * 12) / 2;
    gfx->setCursor(xPos, 115);
    gfx->print(versionText);

    dibujarBoton(20, 155, 132, 35, 8, MAT_CONNECT, "Actualizar", 1, BGR_WHITE);
    dibujarBoton(20, 200, 132, 35, 8, MAT_OFFLINE, "Mas tarde", 1, BGR_WHITE);

    uint16_t tx, ty;
    while (true) {
        if (leerTouch(tx, ty)) {
            Serial.printf("[UPDATE] Popup - Touch en x=%d, y=%d\n", tx, ty);

            if (tx >= 20 && tx <= 152 && ty >= 155 && ty <= 190) {
                Serial.println("[UPDATE] Botón 'Actualizar' presionado");
                showConfirmPopup();
                return;
            }
            if (tx >= 20 && tx <= 152 && ty >= 200 && ty <= 235) {
                Serial.println("[UPDATE] Botón 'Más tarde' presionado");
                updateAvailable = false;
                return;
            }
        }
        delay(50);
    }
}

// --------------- Popup de confirmación ---------------
void showConfirmPopup() {
    Serial.println("[UPDATE] Mostrando popup de confirmación");

    gfx->fillRoundRect(10, 100, 152, 120, 12, MAT_BG);
    gfx->drawRoundRect(10, 100, 152, 120, 12, BGR_WHITE);

    gfx->setTextColor(BGR_YELLOW);
    gfx->setTextSize(1);
    imprimirCentrado("Se reiniciara", 115, 1);
    imprimirCentrado("el dispositivo", 130, 1);
    imprimirCentrado("Continuar?", 145, 1);

    dibujarBoton(20, 170, 55, 35, 8, MAT_CONNECT, "Si", 1, BGR_WHITE);
    dibujarBoton(97, 170, 55, 35, 8, MAT_OFFLINE, "No", 1, BGR_WHITE);

    uint16_t tx, ty;
    while (true) {
        if (leerTouch(tx, ty)) {
            Serial.printf("[UPDATE] Confirm - Touch en x=%d, y=%d\n", tx, ty);

            if (tx >= 20 && tx <= 75 && ty >= 170 && ty <= 205) {
                Serial.println("[UPDATE] Botón 'Sí' presionado - INICIANDO OTA");
                performFirmwareUpdate();
                return;
            }
            if (tx >= 97 && tx <= 152 && ty >= 170 && ty <= 205) {
                Serial.println("[UPDATE] Botón 'No' presionado");
                updateAvailable = false;
                return;
            }
        }
        delay(50);
    }
}

// --------------- Obtener URL de asset desde GitHub API ---------------
String getAssetUrl(String assetName) {
    Serial.printf("[UPDATE] Buscando asset '%s'...\n", assetName.c_str());

    HTTPClient *http = new HTTPClient();
    String apiUrl = "https://api.github.com/repos/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) + "/releases/latest";
    http->begin(apiUrl);
    http->addHeader("User-Agent", "Tamagotchi-ESP32");
    http->setTimeout(10000);

    int code = http->GET();
    Serial.printf("[UPDATE] API status: %d\n", code);
    if (code != 200) {
        http->end();
        delete http;
        return "";
    }

    String payload = http->getString();
    http->end();
    delete http;

    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.printf("[UPDATE] JSON error: %s\n", error.c_str());
        return "";
    }

    JsonArray assets = doc["assets"].as<JsonArray>();
    Serial.printf("[UPDATE] %d assets encontrados\n", assets.size());

    for (JsonObject asset : assets) {
        const char* name = asset["name"].as<const char*>();
        Serial.printf("[UPDATE]   - %s\n", name);
        if (String(name) == assetName) {
            String url = asset["browser_download_url"].as<String>();
            Serial.printf("[UPDATE] URL encontrada: %s\n", url.c_str());
            return url;
        }
    }
    Serial.printf("[UPDATE] Asset '%s' NO encontrado\n", assetName.c_str());
    return "";
}

// --------------- Aplicar OTA ---------------
void performFirmwareUpdate() {
    Serial.println("[OTA] Iniciando actualización de firmware...");
    updateInProgress = true;

    gfx->fillScreen(MAT_BG);
    gfx->setTextColor(BGR_WHITE);
    gfx->setTextSize(1);
    imprimirCentrado("Descargando firmware...", 100, 1);

    String downloadUrl = getAssetUrl("firmware.bin");
    if (downloadUrl == "") {
        Serial.println("[OTA] ERROR: firmware.bin no encontrado en assets");
        gfx->setTextColor(BGR_RED);
        imprimirCentrado("Firmware no encontrado", 130, 1);
        delay(2000);
        updateInProgress = false;
        return;
    }

    HTTPClient http;
    http.begin(downloadUrl);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");

    int code = http.GET();
    Serial.printf("[OTA] Descarga firmware.bin, HTTP code: %d\n", code);
    if (code != 200) {
        http.end();
        Serial.println("[OTA] ERROR: Falló la descarga del firmware");
        gfx->setTextColor(BGR_RED);
        imprimirCentrado("Error descarga", 130, 1);
        delay(2000);
        updateInProgress = false;
        return;
    }

    int contentLength = http.getSize();
    Serial.printf("[OTA] Tamaño del firmware: %d bytes\n", contentLength);

    if (!Update.begin(contentLength)) {
        http.end();
        Serial.println("[OTA] ERROR: Update.begin() falló");
        gfx->setTextColor(BGR_RED);
        imprimirCentrado("Error OTA begin", 130, 1);
        delay(2000);
        updateInProgress = false;
        return;
    }

    Serial.println("[OTA] Descargando y escribiendo...");
    int progress = 0, bytesWritten = 0;
    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[512];

    while (http.connected() && bytesWritten < contentLength) {
        size_t available = stream->available();
        if (available) {
            int bytesRead = stream->readBytes(buffer, min((size_t)512, available));
            Update.write(buffer, bytesRead);
            bytesWritten += bytesRead;

            int newProgress = (bytesWritten * 100) / contentLength;
            if (newProgress != progress) {
                progress = newProgress;
                Serial.printf("[OTA] Progreso: %d%% (%d/%d)\n", progress, bytesWritten, contentLength);
                gfx->fillRect(20, 150, 132, 20, MAT_BG);
                gfx->drawRect(20, 150, 132, 20, BGR_WHITE);
                gfx->fillRect(22, 152, (128 * progress) / 100, 16, MAT_CONNECT);
                gfx->setCursor(50, 153);
                gfx->setTextColor(BGR_WHITE);
                gfx->printf("%d%%", progress);
            }
        }
        delay(10);
    }

    http.end();
    Serial.printf("[OTA] Bytes escritos: %d / %d\n", bytesWritten, contentLength);

    if (Update.end()) {
        Serial.println("[OTA] ¡Update.end() exitoso! Guardando versión...");
        File file = SD_MMC.open(CURRENT_VERSION_FILE, "w");
        if (file) {
            file.println(latestVersion);
            file.close();
            Serial.printf("[OTA] %s actualizado a %s\n", CURRENT_VERSION_FILE, latestVersion.c_str());
        }

        gfx->fillScreen(MAT_BG);
        gfx->setTextColor(BGR_GREEN);
        imprimirCentrado("Actualizado!", 140, 2);
        imprimirCentrado("Reiniciando...", 170, 1);
        delay(2000);
        ESP.restart();
    } else {
        Serial.println("[OTA] ERROR: Update.end() falló");
        gfx->fillScreen(MAT_BG);
        gfx->setTextColor(BGR_RED);
        imprimirCentrado("Error OTA!", 140, 2);
        delay(2000);
        updateInProgress = false;
    }
}

// --------------- Descargar y extraer archivos TAR ---------------
void performSDUpdate() {
    Serial.println("[SD] performSDUpdate() llamada");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[SD] No hay WiFi, saliendo");
        return;
    }

    String current = getCurrentVersion();
    String latest = latestVersion;
    if (latest == "") latest = current;

    Serial.printf("[SD] Versión actual SD: %s\n", current.c_str());
    Serial.printf("[SD] Versión firmware: %s\n", latest.c_str());

    if (current == latest && latest != "v0.0.0") {
        Serial.println("[SD] SD ya está actualizada");
        return;
    }

    gfx->fillScreen(MAT_BG);
    gfx->setTextColor(BGR_WHITE);
    gfx->setTextSize(1);
    imprimirCentrado("Actualizando SD...", 100, 1);

    String downloadUrl = getAssetUrl("sd_files.tar");
    if (downloadUrl == "") {
        Serial.println("[SD] No se encontró sd_files.tar, solo actualizando version.txt");
        updateVersionFile();
        return;
    }

    // Usar HTTPClient en el heap
    HTTPClient *http = new HTTPClient();
    http->begin(downloadUrl);
    http->addHeader("User-Agent", "Tamagotchi-ESP32");
    http->setTimeout(30000);

    int code = http->GET();
    Serial.printf("[SD] Descarga sd_files.tar, HTTP code: %d\n", code);
    if (code != 200) {
        http->end();
        delete http;
        Serial.println("[SD] ERROR: Falló la descarga del TAR");
        return;
    }

    WiFiClient *stream = http->getStreamPtr();
    File tarFile = SD_MMC.open("/update.tar", "w");
    if (!tarFile) {
        http->end();
        delete http;
        Serial.println("[SD] ERROR: No se pudo crear /update.tar");
        return;
    }

    // Buffer pequeño en stack, procesamiento por chunks
    uint8_t buffer[256];
    int contentLength = http->getSize();
    int downloaded = 0;
    Serial.printf("[SD] Descargando TAR (%d bytes)...\n", contentLength);

    while (http->connected() && downloaded < contentLength) {
        size_t available = stream->available();
        if (available) {
            int bytesRead = stream->readBytes(buffer, min((size_t)256, available));
            tarFile.write(buffer, bytesRead);
            downloaded += bytesRead;
        }
        yield(); // Permitir que el watchdog respire
    }
    tarFile.close();
    http->end();
    delete http;
    Serial.printf("[SD] TAR descargado: %d bytes\n", downloaded);

    Serial.println("[SD] Extrayendo TAR...");
    extractTar("/update.tar", "/");
    SD_MMC.remove("/update.tar");

    updateVersionFile();
    Serial.println("[SD] Actualización de SD completada");

    gfx->setTextColor(BGR_GREEN);
    imprimirCentrado("SD actualizada!", 160, 2);
    delay(1500);
}

// --------------- Extraer archivo TAR ---------------
void extractTar(const char* tarPath, const char* destDir) {
    File tarFile = SD_MMC.open(tarPath, "r");
    if (!tarFile) {
        Serial.println("[TAR] ERROR: No se pudo abrir el archivo TAR");
        return;
    }

    Serial.printf("[TAR] Abriendo %s (%d bytes)\n", tarPath, tarFile.size());

    TarHeader header;
    int fileCount = 0;

    while (tarFile.read((uint8_t*)&header, sizeof(TarHeader)) == sizeof(TarHeader)) {
        if (header.name[0] == 0) {
            Serial.println("[TAR] Fin del archivo (header vacío)");
            break;
        }

        // Calcular tamaño del archivo (en octal)
        size_t fileSize = 0;
        for (int i = 0; i < 11 && header.size[i] != 0 && header.size[i] != ' '; i++) {
            if (header.size[i] >= '0' && header.size[i] <= '7') {
                fileSize = fileSize * 8 + (header.size[i] - '0');
            }
        }

        String filename = String(destDir) + String(header.name);
        // Limpiar caracteres nulos
        filename.replace("\0", "");

        if (header.typeflag == '5') {
            Serial.printf("[TAR] Directorio: %s\n", filename.c_str());
            if (!SD_MMC.exists(filename)) {
                SD_MMC.mkdir(filename);
            }
        } else if (fileSize > 0) {
            fileCount++;
            Serial.printf("[TAR] Extrayendo: %s (%d bytes)\n", filename.c_str(), fileSize);

            if (SD_MMC.exists(filename)) {
                SD_MMC.remove(filename);
            }

            File outFile = SD_MMC.open(filename, "w");
            if (outFile) {
                uint8_t buf[512];
                size_t remaining = fileSize;

                while (remaining > 0) {
                    size_t toRead = min((size_t)512, remaining);
                    size_t read = tarFile.read(buf, toRead);
                    if (read == 0) break;
                    outFile.write(buf, read);
                    remaining -= read;
                }
                outFile.close();
                Serial.printf("[TAR]   -> OK\n");
            } else {
                Serial.printf("[TAR]   -> ERROR al crear archivo\n");
                tarFile.seek(tarFile.position() + fileSize);
            }
        }

        // Alinear a bloque de 512 bytes
        size_t blocks = (fileSize + 511) / 512;
        size_t pos = tarFile.position();
        size_t aligned = ((pos + 511) / 512) * 512;
        if (aligned != pos) {
            tarFile.seek(aligned);
        }
    }

    tarFile.close();
    Serial.printf("[TAR] Extracción completa: %d archivos\n", fileCount);
}

// --------------- Actualizar archivo de versión ---------------
void updateVersionFile() {
    File file = SD_MMC.open(CURRENT_VERSION_FILE, "w");
    if (file) {
        file.println(latestVersion);
        file.close();
        Serial.printf("[SD] %s actualizado a: %s\n", CURRENT_VERSION_FILE, latestVersion.c_str());
    } else {
        Serial.printf("[SD] ERROR: No se pudo escribir %s\n", CURRENT_VERSION_FILE);
    }
}
