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
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.setTimeout(10000);
    String url = "https://api.github.com/repos/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) + "/releases/latest";
    http.begin(url);
    
    http.addHeader("User-Agent", "Tamagotchi-ESP32");

    int httpCode = http.GET();
    if (httpCode != 200) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) return false;

    latestVersion = doc["tag_name"].as<String>();
    
    String current = getCurrentVersion();
    if (latestVersion != current && latestVersion.length() > 0) {
        updateAvailable = true;
        return true;
    }
    return false;
}

// --------------- Leer versión actual ---------------
String getCurrentVersion() {
    if (!SD_MMC.exists(CURRENT_VERSION_FILE)) return "v0.0.0";
    
    File file = SD_MMC.open(CURRENT_VERSION_FILE, "r");
    if (!file) return "v0.0.0";
    
    String version = file.readStringUntil('\n');
    version.trim();
    file.close();
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
    return (x >= 8 && x <= 32 && y >= 8 && y <= 32);
}

// --------------- Popup de actualización ---------------
void showUpdatePopup() {
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
            if (tx >= 20 && tx <= 152 && ty >= 155 && ty <= 190) {
                showConfirmPopup();
                return;
            }
            if (tx >= 20 && tx <= 152 && ty >= 200 && ty <= 235) {
                updateAvailable = false;
                return;
            }
        }
        delay(50);
    }
}

// --------------- Popup de confirmación ---------------
void showConfirmPopup() {
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
            if (tx >= 20 && tx <= 75 && ty >= 170 && ty <= 205) {
                performFirmwareUpdate();
                return;
            }
            if (tx >= 97 && tx <= 152 && ty >= 170 && ty <= 205) {
                updateAvailable = false;
                return;
            }
        }
        delay(50);
    }
}

// --------------- Obtener URL de asset desde GitHub API ---------------
String getAssetUrl(String assetName) {
    HTTPClient http;
    String apiUrl = "https://api.github.com/repos/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) + "/releases/latest";
    http.begin(apiUrl);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");
    
    if (http.GET() != 200) {
        http.end();
        return "";
    }
    
    String payload = http.getString();
    http.end();
    
    StaticJsonDocument<4096> doc;
    deserializeJson(doc, payload);
    JsonArray assets = doc["assets"].as<JsonArray>();
    
    for (JsonObject asset : assets) {
        if (String(asset["name"].as<const char*>()) == assetName) {
            return asset["browser_download_url"].as<String>();
        }
    }
    return "";
}

// --------------- Aplicar OTA ---------------
void performFirmwareUpdate() {
    gfx->fillScreen(MAT_BG);
    gfx->setTextColor(BGR_WHITE);
    gfx->setTextSize(1);
    imprimirCentrado("Descargando firmware...", 100, 1);

    String downloadUrl = getAssetUrl("firmware.bin");
    if (downloadUrl == "") {
        gfx->setTextColor(BGR_RED);
        imprimirCentrado("Firmware no encontrado", 130, 1);
        delay(2000);
        return;
    }

    HTTPClient http;
    http.begin(downloadUrl);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");
    
    if (http.GET() != 200) {
        http.end();
        gfx->setTextColor(BGR_RED);
        imprimirCentrado("Error descarga", 130, 1);
        delay(2000);
        return;
    }

    int contentLength = http.getSize();
    if (!Update.begin(contentLength)) {
        http.end();
        gfx->setTextColor(BGR_RED);
        imprimirCentrado("Error OTA begin", 130, 1);
        delay(2000);
        return;
    }

    int progress = 0, bytesWritten = 0;
    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[1024];
    
    while (http.connected() && bytesWritten < contentLength) {
        size_t available = stream->available();
        if (available) {
            int bytesRead = stream->readBytes(buffer, min((size_t)1024, available));
            Update.write(buffer, bytesRead);
            bytesWritten += bytesRead;
            
            int newProgress = (bytesWritten * 100) / contentLength;
            if (newProgress != progress) {
                progress = newProgress;
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
    
    if (Update.end()) {
        File file = SD_MMC.open(CURRENT_VERSION_FILE, "w");
        if (file) {
            file.println(latestVersion);
            file.close();
        }
        
        gfx->fillScreen(MAT_BG);
        gfx->setTextColor(BGR_GREEN);
        imprimirCentrado("Actualizado!", 140, 2);
        imprimirCentrado("Reiniciando...", 170, 1);
        delay(2000);
        ESP.restart();
    } else {
        gfx->fillScreen(MAT_BG);
        gfx->setTextColor(BGR_RED);
        imprimirCentrado("Error OTA!", 140, 2);
        delay(2000);
    }
}

// --------------- Descargar y extraer archivos TAR ---------------
void performSDUpdate() {
    if (WiFi.status() != WL_CONNECTED) return;

    String current = getCurrentVersion();
    String latest = latestVersion;
    if (latest == "") latest = current; // si no se ha chequeado aún
    
    // Si la versión de la SD ya es la misma que el firmware, no actualizar
    if (current == latest) return;

    gfx->fillScreen(MAT_BG);
    gfx->setTextColor(BGR_WHITE);
    gfx->setTextSize(1);
    imprimirCentrado("Actualizando SD...", 100, 1);

    String downloadUrl = getAssetUrl("sd_files.tar");
    if (downloadUrl == "") {
        // No hay TAR, no es error fatal
        updateVersionFile();
        return;
    }

    HTTPClient http;
    http.begin(downloadUrl);
    http.addHeader("User-Agent", "Tamagotchi-ESP32");
    
    if (http.GET() != 200) {
        http.end();
        return;
    }

    // Descargar tar a archivo temporal
    WiFiClient *stream = http.getStreamPtr();
    File tarFile = SD_MMC.open("/update.tar", "w");
    if (!tarFile) {
        http.end();
        return;
    }

    uint8_t buffer[512];
    int contentLength = http.getSize();
    int downloaded = 0;
    
    while (http.connected() && downloaded < contentLength) {
        size_t available = stream->available();
        if (available) {
            int bytesRead = stream->readBytes(buffer, min((size_t)512, available));
            tarFile.write(buffer, bytesRead);
            downloaded += bytesRead;
        }
        delay(10);
    }
    tarFile.close();
    http.end();

    // Extraer TAR
    extractTar("/update.tar", "/");
    SD_MMC.remove("/update.tar");
    
    // Actualizar version.txt
    updateVersionFile();
    
    gfx->setTextColor(BGR_GREEN);
    imprimirCentrado("SD actualizada!", 160, 2);
    delay(1500);
}

// --------------- Extraer archivo TAR ---------------
void extractTar(const char* tarPath, const char* destDir) {
    File tarFile = SD_MMC.open(tarPath, "r");
    if (!tarFile) return;

    TarHeader header;
    
    while (tarFile.read((uint8_t*)&header, sizeof(TarHeader)) == sizeof(TarHeader)) {
        // Verificar si el header está vacío (fin del archivo)
        if (header.name[0] == 0) break;
        
        // Calcular tamaño del archivo (en octal en el header)
        size_t fileSize = 0;
        for (int i = 0; i < 11 && header.size[i] != 0; i++) {
            fileSize = fileSize * 8 + (header.size[i] - '0');
        }
        
        String filename = String(destDir) + "/" + String(header.name);
        
        if (header.typeflag == '5' || filename.endsWith("/")) {
            // Es un directorio
            if (!SD_MMC.exists(filename)) {
                SD_MMC.mkdir(filename);
            }
        } else if (fileSize > 0) {
            // Es un archivo
            // Borrar si existe
            if (SD_MMC.exists(filename)) {
                SD_MMC.remove(filename);
            }
            
            File outFile = SD_MMC.open(filename, "w");
            if (outFile) {
                uint8_t buf[512];
                size_t remaining = fileSize;
                
                while (remaining > 0) {
                    size_t toRead = min((size_t)512, remaining);
                    tarFile.read(buf, toRead);
                    outFile.write(buf, toRead);
                    remaining -= toRead;
                }
                outFile.close();
            } else {
                // Saltar este archivo
                tarFile.seek(tarFile.position() + fileSize);
            }
        }
        
        // Los datos en TAR están alineados a bloques de 512 bytes
        size_t blocks = (fileSize + 511) / 512;
        tarFile.seek((tarFile.position() + 511) / 512 * 512);
    }
    
    tarFile.close();
}

// --------------- Actualizar archivo de versión ---------------
void updateVersionFile() {
    File file = SD_MMC.open(CURRENT_VERSION_FILE, "w");
    if (file) {
        file.println(latestVersion);
        file.close();
    }
}
