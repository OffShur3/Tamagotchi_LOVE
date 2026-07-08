// Librerias
#include "UpdateManager.h"
#include "NetworkManager.h"
#include <ArduinoJson.h>

// Archivos
#include "colors.h"
#include "UI.h"

bool updateAvailable = false;
String latestVersion = "";
bool updateInProgress = false;

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
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");

    int httpCode = http.GET();
    Serial.printf("[UPDATE] HTTP code: %d\n", httpCode);

    if (httpCode != 200) {
        Serial.printf("[UPDATE] Error HTTP %d, saliendo\n", httpCode);
        http.end();
        return false;
    }

    // Obtener el payload
    String payload = http.getString();
    http.end();
    Serial.printf("[UPDATE] Respuesta recibida (%d bytes)\n", payload.length());

    // Buscar tag_name manualmente (sin JSON parser, para evitar problemas de memoria)
    int tagPos = payload.indexOf("\"tag_name\":\"");
    if (tagPos == -1) {
        Serial.println("[UPDATE] ERROR: No se encontró tag_name");
        return false;
    }
    
    tagPos += 12; // longitud de "tag_name":"
    int tagEnd = payload.indexOf("\"", tagPos);
    latestVersion = payload.substring(tagPos, tagEnd);
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
    if (!SD_MMC.exists(CURRENT_VERSION_FILE)) {
        Serial.println("[UPDATE] version.txt no existe, devolviendo v0.0.0");
        return "v0.0.0";
    }

    File file = SD_MMC.open(CURRENT_VERSION_FILE, "r");
    if (!file) {
        Serial.println("[UPDATE] Error al abrir version.txt");
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

    // Esperar a que suelte antes de empezar a detectar
    esperarSoltar();

    uint16_t tx, ty;
    while (true) {
        if (leerTouch(tx, ty)) {
            Serial.printf("[UPDATE] Popup - Touch en x=%d, y=%d\n", tx, ty);

            if (tx >= 20 && tx <= 152 && ty >= 155 && ty <= 190) {
                Serial.println("[UPDATE] Botón 'Actualizar' presionado");
                esperarSoltar();
                showConfirmPopup();
                return;
            }
            if (tx >= 20 && tx <= 152 && ty >= 200 && ty <= 235) {
                Serial.println("[UPDATE] Botón 'Más tarde' presionado");
                esperarSoltar();

                // Restaurar la pantalla
                gfx->fillScreen(MAT_BG);
                if (game) game->redraw();
                
                return;  // badge seguirá visible porque updateAvailable sigue true
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

    // Esperar a que suelte antes de empezar a detectar
    esperarSoltar();

    uint16_t tx, ty;
    while (true) {
        if (leerTouch(tx, ty)) {
            Serial.printf("[UPDATE] Confirm - Touch en x=%d, y=%d\n", tx, ty);

            if (tx >= 20 && tx <= 75 && ty >= 170 && ty <= 205) {
                Serial.println("[UPDATE] Botón 'Sí' presionado - INICIANDO OTA");
                esperarSoltar();
                performFirmwareUpdate();
                return;
            }
            if (tx >= 97 && tx <= 152 && ty >= 170 && ty <= 205) {
                Serial.println("[UPDATE] Botón 'No' presionado");
                esperarSoltar();
                updateAvailable = false;
                return;
            }
        }
        delay(50);
    }
}

// --------------- Obtener URL de asset desde GitHub API ---------------
String getAssetUrl(const char* assetName) {
    Serial.printf("[UPDATE] Buscando asset '%s'...\n", assetName);

    HTTPClient http;
    http.setTimeout(10000);
    String apiUrl = "https://api.github.com/repos/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) + "/releases/latest";
    http.begin(apiUrl);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);


    int code = http.GET();
    Serial.printf("[UPDATE] API status: %d\n", code);
    if (code != 200) {
        http.end();
        return "";
    }

    // Leer el payload por partes para no cargar todo en RAM
    // Buscar "browser_download_url" manualmente
    String result = "";
    WiFiClient *stream = http.getStreamPtr();
    
    // Leer todo el body
    String body = "";
    while (http.connected()) {
        size_t available = stream->available();
        if (available) {
            uint8_t buf[256];
            int read = stream->readBytes(buf, min((size_t)256, available));
            body += String((char*)buf).substring(0, read);
        }
        if (body.length() > 10000) break; // Seguridad
    }
    http.end();

    // Buscar el asset por nombre
    int assetPos = body.indexOf(String("\"name\":\"") + assetName + "\"");
    if (assetPos == -1) {
        Serial.printf("[UPDATE] Asset '%s' NO encontrado\n", assetName);
        return "";
    }

    // Buscar browser_download_url cerca de ese asset
    int urlPos = body.indexOf("\"browser_download_url\":\"", assetPos);
    if (urlPos == -1) {
        Serial.println("[UPDATE] URL no encontrada");
        return "";
    }
    
    urlPos += 24; // longitud de "browser_download_url":"
    int urlEnd = body.indexOf("\"", urlPos);
    result = body.substring(urlPos, urlEnd);
    result.replace("\\/", "/"); // GitHub escapa las / en JSON
    
    Serial.printf("[UPDATE] URL encontrada: %s\n", result.c_str());
    return result;
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
        Serial.println("[OTA] ERROR: firmware.bin no encontrado");
        gfx->setTextColor(BGR_RED);
        imprimirCentrado("Firmware no encontrado", 130, 1);
        delay(2000);
        updateInProgress = false;
        return;
    }

    HTTPClient http;
    http.begin(downloadUrl);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 

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
                Serial.printf("[OTA] Progreso: %d%%\n", progress);
                gfx->fillRect(20, 150, 132, 20, MAT_BG);
                gfx->drawRect(20, 150, 132, 20, BGR_WHITE);
                gfx->fillRect(22, 152, (128 * progress) / 100, 16, MAT_CONNECT);
                gfx->setCursor(50, 153);
                gfx->setTextColor(BGR_WHITE);
                gfx->printf("%d%%", progress);
            }
        }
        yield();
    }

    http.end();
    Serial.printf("[OTA] Bytes escritos: %d / %d\n", bytesWritten, contentLength);

    if (Update.end()) {
        Serial.println("[OTA] ¡Update.end() exitoso!");
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
        Serial.println("[SD] No se encontró sd_files.tar, actualizando version.txt");
        updateVersionFile();
        return;
    }

    HTTPClient http;
    http.setTimeout(30000);
    http.begin(downloadUrl);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int code = http.GET();
    Serial.printf("[SD] Descarga sd_files.tar, HTTP code: %d\n", code);
    if (code != 200) {
        http.end();
        Serial.println("[SD] ERROR: Falló la descarga del TAR");
        return;
    }

    WiFiClient *stream = http.getStreamPtr();
    File tarFile = SD_MMC.open("/update.tar", "w");
    if (!tarFile) {
        http.end();
        Serial.println("[SD] ERROR: No se pudo crear /update.tar");
        return;
    }

    uint8_t buffer[256];
    int contentLength = http.getSize();
    int downloaded = 0;
    Serial.printf("[SD] Descargando TAR (%d bytes)...\n", contentLength);

    while (http.connected() && downloaded < contentLength) {
        size_t available = stream->available();
        if (available) {
            int bytesRead = stream->readBytes(buffer, min((size_t)256, available));
            tarFile.write(buffer, bytesRead);
            downloaded += bytesRead;
        }
        yield();
    }
    tarFile.close();
    http.end();
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

    uint8_t header[512];
    int fileCount = 0;

    while (tarFile.read(header, 512) == 512) {
        if (header[0] == 0) {
            Serial.println("[TAR] Fin del archivo (header vacío)");
            break;
        }

        // Extraer nombre (bytes 0-99)
        char name[101];
        memcpy(name, header, 100);
        name[100] = '\0';

        // Limpiar "./" al inicio si existe
        if (name[0] == '.' && name[1] == '/') {
            memmove(name, name + 2, strlen(name + 2) + 1);
        }

        // Si después de limpiar queda vacío, es el directorio raíz "./", lo saltamos
        if (strlen(name) == 0) {
            size_t fileSize = 0;
            for (int i = 0; i < 12; i++) {
                if (header[124 + i] >= '0' && header[124 + i] <= '7')
                    fileSize = fileSize * 8 + (header[124 + i] - '0');
            }
            size_t blocks = (fileSize + 511) / 512;
            size_t pos = tarFile.position();
            size_t aligned = ((pos + 511) / 512) * 512;
            if (aligned != pos) tarFile.seek(aligned);
            continue;
        }

        // Tamaño
        size_t fileSize = 0;
        for (int i = 0; i < 11; i++) {
            if (header[124 + i] >= '0' && header[124 + i] <= '7')
                fileSize = fileSize * 8 + (header[124 + i] - '0');
        }

        char typeflag = header[156];
        String fullPath = String(destDir) + name;

        if (typeflag == '5' || (fileSize == 0 && fullPath.endsWith("/"))) {
            Serial.printf("[TAR] Directorio: %s\n", fullPath.c_str());
            if (!SD_MMC.exists(fullPath)) {
                SD_MMC.mkdir(fullPath);
            }
        } else if (fileSize > 0) {
            fileCount++;
            Serial.printf("[TAR] Extrayendo: %s (%d bytes)\n", fullPath.c_str(), fileSize);

            if (SD_MMC.exists(fullPath)) {
                SD_MMC.remove(fullPath);
            }

            File outFile = SD_MMC.open(fullPath, "w");
            if (outFile) {
                uint8_t buf[256];
                size_t remaining = fileSize;
                while (remaining > 0) {
                    size_t toRead = min((size_t)256, remaining);
                    size_t r = tarFile.read(buf, toRead);
                    if (r == 0) break;
                    outFile.write(buf, r);
                    remaining -= r;
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
        if (aligned != pos) tarFile.seek(aligned);
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

// --------------- Verificar si necesita actualización obligatoria ---------------
bool needMandatoryUpdate() {
    // 1. ¿Faltan archivos esenciales en la SD?
    if (!SD_MMC.exists("/QR Network.png")) {
        Serial.println("[UPDATE] Faltan archivos SD - Actualización obligatoria");
        return true;
    }
    
    // 2. ¿No existe el archivo de versión?
    if (!SD_MMC.exists(CURRENT_VERSION_FILE)) {
        Serial.println("[UPDATE] No hay version.txt - Actualización obligatoria");
        return true;
    }
    
    String current = getCurrentVersion();
    // 3. ¿La versión actual es la predeterminada (v0.0.0)?
    if (current == "v0.0.0") {
        Serial.println("[UPDATE] Versión actual v0.0.0 - Actualización obligatoria");
        return true;
    }
    
    // 4. ¿Hay una versión más reciente en GitHub?
    if (latestVersion != "" && current != latestVersion) {
        Serial.printf("[UPDATE] Versión actual %s, disponible %s - Actualización obligatoria\n",
                      current.c_str(), latestVersion.c_str());
        return true;
    }
    
    // Si todo está en orden, no es necesario actualizar
    return false;
}

// --------------- Pantalla de progreso genérica ---------------
void drawProgressScreen(const String& title, int progress, int total, bool redrawAll) {
    static String lastTitle = "";
    if (redrawAll || lastTitle != title) {
        gfx->fillScreen(MAT_BG);
        lastTitle = title;
        
        // Título centrado
        gfx->setTextColor(BGR_WHITE);
        gfx->setTextSize(1);
        int titleWidth = title.length() * 6;
        int titleX = (gfx->width() - titleWidth) / 2;
        gfx->setCursor(titleX, 80);
        gfx->print(title);
    }
    
    int barX = 20, barY = 130, barW = 132, barH = 20;
    gfx->drawRect(barX, barY, barW, barH, BGR_WHITE);
    
    int filled = 0;
    if (total > 0) {
        filled = (barW - 4) * progress / total;
    }
    if (filled > 0) {
        gfx->fillRect(barX + 2, barY + 2, filled, barH - 4, MAT_CONNECT);
    }
    if (filled < barW - 4) {
        gfx->fillRect(barX + 2 + filled, barY + 2, (barW - 4) - filled, barH - 4, MAT_BG);
    }
    
    gfx->setTextColor(BGR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(barX + 40, barY + barH + 10);
    if (total > 0) {
        int pct = (progress * 100) / total;
        gfx->print(String(pct) + "%");
    } else {
        gfx->print("...");
    }
}

// --------------- Actualización completa (SD + OTA) ---------------
void performFullUpdate() {
    updateInProgress = true;

    // Obtener latestVersion si no la tenemos
    if (latestVersion == "") {
        HTTPClient http;
        http.setTimeout(10000);
        String url = "https://api.github.com/repos/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) + "/releases/latest";
        http.begin(url);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.addHeader("User-Agent", "Tamagotchi-ESP32");
        int httpCode = http.GET();
        if (httpCode == 200) {
            String payload = http.getString();
            int tagPos = payload.indexOf("\"tag_name\":\"");
            if (tagPos != -1) {
                tagPos += 12;
                int tagEnd = payload.indexOf("\"", tagPos);
                latestVersion = payload.substring(tagPos, tagEnd);
            }
        }
        http.end();
    }

    // ===== PASO 1: Descargar sd_files.tar =====
    drawProgressScreen("Descargando SD...", 0, 1, true);
    String sdUrl = getAssetUrl("sd_files.tar");
    if (sdUrl == "") {
        drawProgressScreen("Error: SD no disponible", 0, 1, true);
        delay(3000);
        updateInProgress = false;
        return;
    }

    HTTPClient http;
    http.setTimeout(30000);
    http.begin(sdUrl);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int code = http.GET();
    if (code != 200) {
        http.end();
        drawProgressScreen("Error al descargar SD", 0, 1, true);
        delay(3000);
        updateInProgress = false;
        return;
    }

    int contentLength = http.getSize();
    WiFiClient *stream = http.getStreamPtr();
    File tarFile = SD_MMC.open("/update.tar", "w");
    if (!tarFile) {
        http.end();
        drawProgressScreen("Error al crear /update.tar", 0, 1, true);
        delay(3000);
        updateInProgress = false;
        return;
    }

    uint8_t buffer[256];
    int downloaded = 0;
    while (http.connected() && downloaded < contentLength) {
        size_t available = stream->available();
        if (available) {
            int bytesRead = stream->readBytes(buffer, min((size_t)256, available));
            tarFile.write(buffer, bytesRead);
            downloaded += bytesRead;
            if (contentLength > 0) {
                drawProgressScreen("Descargando SD...", downloaded, contentLength);
            }
        }
        yield();
    }
    tarFile.close();
    http.end();

    // Extraer TAR
    drawProgressScreen("Actualizando SD...", 0, 1, true);
    extractTar("/update.tar", "/");
    SD_MMC.remove("/update.tar");

    // Actualizar version.txt
    updateVersionFile();

    // ===== PASO 2: Descargar firmware.bin y OTA =====
    drawProgressScreen("Descargando firmware...", 0, 1, true);
    String fwUrl = getAssetUrl("firmware.bin");
    if (fwUrl == "") {
        drawProgressScreen("Error: Firmware no disponible", 0, 1, true);
        delay(3000);
        updateInProgress = false;
        return;
    }

    HTTPClient httpFw;
    httpFw.begin(fwUrl);
    httpFw.addHeader("User-Agent", "Tamagotchi-ESP32");
    httpFw.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    code = httpFw.GET();
    if (code != 200) {
        httpFw.end();
        drawProgressScreen("Error al descargar firmware", 0, 1, true);
        delay(3000);
        updateInProgress = false;
        return;
    }

    contentLength = httpFw.getSize();
    if (!Update.begin(contentLength)) {
        httpFw.end();
        drawProgressScreen("Error OTA begin", 0, 1, true);
        delay(3000);
        updateInProgress = false;
        return;
    }

    stream = httpFw.getStreamPtr();
    int bytesWritten = 0;
    uint8_t otaBuffer[512];
    while (httpFw.connected() && bytesWritten < contentLength) {
        size_t available = stream->available();
        if (available) {
            int bytesRead = stream->readBytes(otaBuffer, min((size_t)512, available));
            Update.write(otaBuffer, bytesRead);
            bytesWritten += bytesRead;
            if (contentLength > 0) {
                drawProgressScreen("Descargando firmware...", bytesWritten, contentLength);
            }
        }
        yield();
    }
    httpFw.end();

    if (Update.end()) {
        File versionFile = SD_MMC.open(CURRENT_VERSION_FILE, "w");
        if (versionFile) {
            versionFile.println(latestVersion);
            versionFile.close();
        }
        drawProgressScreen("Actualizado! Reiniciando...", 1, 1, true);
        delay(2000);
        ESP.restart();
    } else {
        drawProgressScreen("Error OTA final", 0, 1, true);
        delay(3000);
    }

    updateInProgress = false;
}
