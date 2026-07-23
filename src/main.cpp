// src/main.cpp
#include <Arduino.h>
#include <SPI.h>
#include <SD_MMC.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <PNGdec.h>
#include <WiFiMulti.h> 
#include <ArduinoJson.h> 
#include <esp_sntp.h> // <-- Callback de NTP nativo
#include "Game.h"
#include "assets/AssetManager.h"
#include "render/SceneManager.h"
#include "core/TamaNetworkManager.h" 
#include "core/UpdateManager.h"
#include "core/touch_axs5106.h"
#include "tamagotchi/ClockWidget.h"
#include "UI.h"

#define TFT_DC   45
#define TFT_CS   21
#define TFT_SCLK 38
#define TFT_MOSI 39
#define TFT_RST  40
#define TFT_BL   46

#define SD_CLK   16
#define SD_CMD   15
#define SD_D0    17

#define BOOT_PIN  0 

#define RETRO_BG    0x1042 
#define RETRO_WHITE 0xFFFF 

enum KernelState {
    STATE_SCANNING,
    STATE_CONNECTING,
    STATE_GAMEPLAY,
    STATE_POPUP,
    STATE_PORTAL,
    STATE_POPUP_UPDATE
};

// --- DECLARACIONES DE FUNCIONES ---
void* pngOpen(const char *filename, int32_t *size);
void pngClose(void *handle);
int32_t pngRead(PNGFILE *handle, uint8_t *buffer, int32_t length);
int32_t pngSeek(PNGFILE *handle, int32_t position);
int qrDrawCallback(PNGDRAW *pDraw);
bool leerTouch(uint16_t &x, uint16_t &y);
bool detectarSwipeRight();
bool checkExitCallback();
int leerClickPopup();
int leerClickPopupUpdate();
bool esperarSD();
void irADormir();
bool cargarRedesSD();
void dibujarPopupStardew();
void dibujarPantallaPortal();

PNG png;
WiFiMulti wifiMulti; 
std::vector<std::pair<String, String>> redesGuardadas;
int intentoRedActual = 0;
unsigned long ultimoIntentoWiFi = 0;

uint16_t globalLineBuffer[512];

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 0, false, 172, 320, 34, 0, 34, 0);

Game* game = nullptr;

bool debugMode = false;
KernelState currentState = STATE_CONNECTING;
uint32_t connectionStartTime = 0;
uint32_t popupLaunchTime = 0; 

bool updateAvailable = false;
String latestVersion = "";
bool updateInProgress = false;
bool mandatoryUpdate = false;

volatile bool peticionDormir = false;

// --- CALLBACK NATIVO ESP-IDF CUANDO EL PAQUETE NTP LLEGA DESDE INTERNET ---
void cbNtpSync(struct timeval *tv) {
    time_t realNow = tv->tv_sec;
    struct tm tInfo;
    localtime_r(&realNow, &tInfo);

    Serial.printf("\n[TIME] ¡EVENTO NTP! Hora real recibida de Internet: [%02d:%02d:%02d]\n", 
                  tInfo.tm_hour, tInfo.tm_min, tInfo.tm_sec);
    
    // Activa el cartel en pantalla
    ClockWidget::isSyncedWithInternet = true;

    if (game) {
        game->onTimeSynced(); // Ajusta el delta de vida acumulado
    }
}

void IRAM_ATTR isrBotonBoot() {
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();
    if (interruptTime - lastInterruptTime > 200) { 
        peticionDormir = true;
    }
    lastInterruptTime = interruptTime;
}

void* pngOpen(const char *filename, int32_t *size) {
    File *f = new File(SD_MMC.open(filename, "r"));
    if (!f || !*f) return nullptr;
    *size = f->size();
    return (void *)f;
}
void pngClose(void *handle) { File *f = (File *)handle; if (f) { f->close(); delete f; } }
int32_t pngRead(PNGFILE *handle, uint8_t *buffer, int32_t length) {
    File *f = (File *)handle->fHandle; return f->read(buffer, length);
}
int32_t pngSeek(PNGFILE *handle, int32_t position) {
    File *f = (File *)handle->fHandle; return f->seek(position) ? position : -1;
}

int qrDrawCallback(PNGDRAW *pDraw) {
    png.getLineAsRGB565(pDraw, globalLineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0);
    int y = pDraw->y + 25; 
    int x_offset = (172 - pDraw->iWidth) / 2; 
    gfx->draw16bitRGBBitmap(x_offset, y, globalLineBuffer, pDraw->iWidth, 1);
    return 1;
}

bool leerTouch(uint16_t &x, uint16_t &y) {
    Wire.beginTransmission(0x63); Wire.write(0x02); Wire.endTransmission();
    uint8_t buf[5];
    if (Wire.requestFrom(0x63, 5) == 5) {
        buf[0] = Wire.read(); buf[1] = Wire.read(); buf[2] = Wire.read(); buf[3] = Wire.read(); buf[4] = Wire.read();
        if (buf[0] > 0 && buf[0] < 3) {
            uint16_t raw_x = ((buf[1] & 0x0F) << 8) | buf[2];
            uint16_t raw_y = ((buf[3] & 0x0F) << 8) | buf[4];
            x = (raw_x > 172) ? 0 : (172 - raw_x);
            y = raw_y;
            return true;
        }
    }
    return false;
}

bool detectarSwipeRight() {
    uint16_t startX = 0, startY = 0;
    uint16_t curX = 0, curY = 0;
    if (leerTouch(startX, startY)) {
        unsigned long startTime = millis();
        while (millis() - startTime < 350) { 
            delay(10);
            if (leerTouch(curX, curY)) {
                if (curX > startX && (curX - startX) > 40) {
                    while (leerTouch(curX, curY)) {
                        delay(10);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

bool checkExitCallback() {
    return detectarSwipeRight();
}

int leerClickPopup() {
    if (millis() - popupLaunchTime < 500) return 0; 
    uint16_t tx, ty;
    if (leerTouch(tx, ty)) {
        int click = UIManager::getStardewPopupClick(tx, ty);
        if (click > 0) {
            unsigned long pressTime = millis();
            while (leerTouch(tx, ty) && (millis() - pressTime < 2000)) delay(10);
            return click;
        }
    }
    return 0;
}

int leerClickPopupUpdate() {
    if (millis() - popupLaunchTime < 500) return 0; 
    uint16_t tx, ty;
    if (leerTouch(tx, ty)) {
        int click = UIManager::getUpdatePopupClick(tx, ty);
        if (click > 0) {
            unsigned long pressTime = millis();
            while (leerTouch(tx, ty) && (millis() - pressTime < 2000)) delay(10);
            return click;
        }
    }
    return 0;
}

bool esperarSD() {
    gfx->fillScreen(RETRO_BG);
    imprimirCentrado(gfx, "SD", 130, 2, RETRO_WHITE);
    imprimirCentrado(gfx, "no detectada", 160, 2, RETRO_WHITE);
    imprimirCentrado(gfx, "Por favor insertela", 190, 1, RETRO_WHITE);
       
    while (true) {
        SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
        if (SD_MMC.begin("/sdcard", true)) return true;
        delay(500);
    }
}

void irADormir() {
    Serial.println("[POWER] Guardando y entrando en Deep Sleep...");
    if (game) game->saveGame(); 

    digitalWrite(TFT_BL, LOW);
    gfx->displayOff();

    while (digitalRead(BOOT_PIN) == LOW) delay(10);
    delay(100);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)BOOT_PIN, 0); 
    esp_deep_sleep_start();
}

bool cargarRedesSD() {
    if (!SD_MMC.exists("/config/wifi.json")) return false;

    File file = SD_MMC.open("/config/wifi.json", "r");
    if (!file) return false;

    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;

    redesGuardadas.clear();
    JsonArray arr = doc.as<JsonArray>();
    
    for (JsonObject obj : arr) {
        const char* ssid = obj["ssid"];
        const char* pass = obj["pass"];
        if (ssid && strlen(ssid) > 0) {
            redesGuardadas.push_back({String(ssid), String(pass)});
            Serial.printf("[KERNEL] Red encolada: %s\n", ssid);
        }
    }
    return !redesGuardadas.empty();
}

void dibujarPopupStardew() { UIManager::drawStardewPopup(gfx); }

void dibujarPantallaPortal() {
    gfx->fillScreen(0x1042); 

    if (!SD_MMC.exists("/QR Network.png")) {
        imprimirCentrado(gfx, "QR no encontrado", 50, 1, 0xF800);
    } else {
        int rc = png.open("/QR Network.png", pngOpen, pngClose, pngRead, pngSeek, qrDrawCallback);
        if (rc == PNG_SUCCESS) {
            png.decode(NULL, 0);
            png.close();
        }
    }

    imprimirCentrado(gfx, "Conecta tu celular a:", 135, 1, 0xFEE0);
    imprimirCentrado(gfx, "TamaConfig", 150, 2, 0xFFFF);
    imprimirCentrado(gfx, "Admite conexion", 185, 1, 0x07E0);
    imprimirCentrado(gfx, "sin Internet", 200, 1, 0x07E0);
    imprimirCentrado(gfx, "Entra al navegador", 230, 1, 0xFFFF);
    imprimirCentrado(gfx, "y elige tu WiFi", 245, 1, 0xFFFF);
    imprimirCentrado(gfx, "<3", 270, 2, 0xFFFF);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    touch_init();

    pinMode(TFT_BL, OUTPUT);
    pinMode(BOOT_PIN, INPUT_PULLUP); 
    attachInterrupt(digitalPinToInterrupt(BOOT_PIN), isrBotonBoot, FALLING);
    digitalWrite(TFT_BL, HIGH);
    
    if (!gfx->begin()) Serial.println("Error inicializando pantalla.");
    
    bus->beginWrite();
    bus->writeCommand(0x36);
    bus->write(0x48); 
    bus->endWrite();

    esperarSD();

    // 1. ZONA HORARIA Y CALLBACK DE NOTIFICACIÓN DE RED
    setenv("TZ", "ART3", 1);
    tzset();
    sntp_set_time_sync_notification_cb(cbNtpSync); // Se ejecutará solo cuando la hora llegue

    // 2. FALLBACK A 08:00 AM SI NO HAY RELOJ PREVIO
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    if (timeinfo->tm_year < 120) { 
        struct tm tm_fallback = {0};
        tm_fallback.tm_year = 2026 - 1900;
        tm_fallback.tm_mon  = 0;
        tm_fallback.tm_mday = 1;
        tm_fallback.tm_hour = 8; // 08:00 AM LOCAL
        tm_fallback.tm_min  = 0;
        tm_fallback.tm_sec  = 0;

        time_t fallback_epoch = mktime(&tm_fallback); 
        struct timeval tv = { .tv_sec = fallback_epoch, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        Serial.println("[TIME] Sin Internet previo. RTC inicializado en fallback: 08:00 AM (ART).");
    }

    // Por defecto ocultamos el reloj hasta confirmar respuesta de Internet
    ClockWidget::isSyncedWithInternet = false;

    String versionArranque = "v0.0.0";
    if (SD_MMC.exists("/config/version.txt")) {
        File vFile = SD_MMC.open("/config/version.txt", "r");
        if (vFile) {
            versionArranque = vFile.readStringUntil('\n');
            versionArranque.trim();
            vFile.close();
        }
    }
    Serial.println("\n-------------------------------");
    Serial.println("Inicializando...");
    Serial.printf("TAMA %s\n", versionArranque.c_str());
    Serial.println("-------------------------------\n");

    if (SD_MMC.exists("/config/debug.txt")) {
        debugMode = true;
        Serial.println("[KERNEL] Archivo /config/debug.txt detectado en la SD. MODO DEBUG ACTIVADO.");
    } else {
        debugMode = false;
    }

    AssetManager::getInstance().setPNG(&png);
    AssetManager::getInstance().setFileSystem(&SD_MMC); 

    game = new Game(172, 320);
    game->init();

    game->tick();
    game->flush(gfx);

    Serial.println("[KERNEL] Iniciando escaneo de redes en background...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    bool tieneRedes = cargarRedesSD();

    if (tieneRedes) {
        WiFi.scanNetworks(true); 
        connectionStartTime = millis();
        ultimoIntentoWiFi = millis();
        currentState = STATE_SCANNING;
    } else {
        WiFi.mode(WIFI_OFF);
        currentState = STATE_POPUP;
        UIManager::drawStardewPopup(gfx);
        popupLaunchTime = millis();
    }
}

void loop() {
    if (peticionDormir) {
        peticionDormir = false; 
        irADormir();
    }

    if (game && (currentState == STATE_GAMEPLAY || currentState == STATE_SCANNING || currentState == STATE_CONNECTING)) {
        game->tick();

        if (updateAvailable && !updateInProgress && currentState == STATE_GAMEPLAY) {
            UIManager::drawUpdateBadgeFB(game->getFramebuffer(), 172, 320, 24, 24, 12); 
        }

        game->flush(gfx);
    }

    switch (currentState) {
        case STATE_SCANNING: {
            int n = WiFi.scanComplete();
            if (n >= 0) {
                std::vector<std::pair<String, String>> redesDisponibles;
                for (int i = 0; i < n; ++i) {
                    String ssid = WiFi.SSID(i);
                    for (const auto& red : redesGuardadas) {
                        if (red.first == ssid) {
                            redesDisponibles.push_back(red);
                            break; 
                        }
                    }
                }
                WiFi.scanDelete(); 

                if (!redesDisponibles.empty()) {
                    redesGuardadas = redesDisponibles; 
                    Serial.printf("[KERNEL] Red al alcance: %s. Conectando...\n", redesGuardadas[0].first.c_str());
                    WiFi.begin(redesGuardadas[0].first.c_str(), redesGuardadas[0].second.c_str());
                    ultimoIntentoWiFi = millis();
                    intentoRedActual = 0;
                    currentState = STATE_CONNECTING;
                } else {
                    Serial.println("[KERNEL] Ninguna red guardada está al alcance. Pasando a Offline.");
                    WiFi.disconnect();
                    WiFi.mode(WIFI_OFF);
                    currentState = STATE_POPUP;
                    UIManager::drawStardewPopup(gfx);
                    popupLaunchTime = millis();
                }
            } else if (n == WIFI_SCAN_FAILED) {
                if (millis() - ultimoIntentoWiFi > 1000) {
                    Serial.println("[KERNEL] Falló el escaneo. Reintentando...");
                    WiFi.scanNetworks(true);
                    ultimoIntentoWiFi = millis();
                }
            }

            if (millis() - connectionStartTime > 15000) {
                Serial.println("[KERNEL] Timeout de escaneo excedido. Lanzando Popup.");
                WiFi.disconnect();
                WiFi.mode(WIFI_OFF);
                currentState = STATE_POPUP;
                UIManager::drawStardewPopup(gfx);
                popupLaunchTime = millis();
            }
            break;
        }

        case STATE_CONNECTING: {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[KERNEL] WiFi conectado con éxito.");
                
                setenv("TZ", "ART3", 1);
                tzset();

                // Usamos Google NTP como primario (Ultra rápido)
                configTzTime("ART3", "time.google.com", "pool.ntp.org", "time.nist.gov");
                Serial.println("[TIME] Solicitando hora real a servidores NTP...");

                // BUCLE ROBUSTO: Esperar hasta 8 segundos para dar tiempo suficiente
                int retry = 0;
                while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retry < 80) {
                    delay(100);
                    retry++;
                }

                if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
                    // El callback cbNtpSync() ya se habrá ejecutado automáticamente
                    Serial.println("[TIME] Sincronización NTP confirmada por el hardware.");
                } else {
                    Serial.println("[TIME] Sin respuesta NTP inicial. Se continuará esperando en background...");
                }

                currentState = STATE_GAMEPLAY;
            } else {
                if (millis() - ultimoIntentoWiFi > 5000) {
                    intentoRedActual++;
                    if (intentoRedActual < redesGuardadas.size()) {
                        WiFi.disconnect();
                        WiFi.begin(redesGuardadas[intentoRedActual].first.c_str(), redesGuardadas[intentoRedActual].second.c_str());
                        ultimoIntentoWiFi = millis();
                    } else {
                        intentoRedActual = 0; 
                        ultimoIntentoWiFi = millis();
                    }
                }
            }

            if (millis() - connectionStartTime > 20000) { 
                Serial.println("[KERNEL] Timeout de WiFi excedido. Lanzando Popup.");
                WiFi.disconnect();
                WiFi.mode(WIFI_OFF);
                currentState = STATE_POPUP;
                UIManager::drawStardewPopup(gfx);
                popupLaunchTime = millis();
            }
            break;
        }

        case STATE_GAMEPLAY: {
            static bool mandatoryUpdateDone = false;
            if (!mandatoryUpdateDone && WiFi.status() == WL_CONNECTED) {
                mandatoryUpdateDone = true;
                
                UpdateManager::Config updateCfg = {
                    .gfx = gfx,
                    .githubUser = "OffShur3",
                    .githubRepo = "Tamagotchi_LOVE",
                    .currentVersionPath = "/config/version.txt",
                    .checkIntervalMs = 300000 
                };
                UpdateManager updateManager(updateCfg);

                Serial.println("[MAIN] Ecosistema Wifi Online. Testeando Estado y Servidor OTA Seguro.");
                
                if (updateManager.checkForUpdate()) {
                    updateAvailable = true;
                    latestVersion = updateManager.getLatestVersion();
                    if (updateManager.needMandatoryUpdate()) mandatoryUpdate = true;
                } else {
                    // Dar un pequeño margen de 1.5s antes de apagar el Wi-Fi por si el paquete NTP venía en camino
                    if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
                        int margin = 0;
                        while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && margin < 15) {
                            delay(100);
                            margin++;
                        }
                    }

                    Serial.println("[MAIN] Confirmación NUBE V. Identica. Sistema Inicia Fase Offline, Extinguiendo Radar");
                    WiFi.disconnect();
                    WiFi.mode(WIFI_OFF);
                }
            }

            if (updateAvailable && !updateInProgress) {
                uint16_t tx, ty;
                if (leerTouch(tx, ty) && UIManager::isTouchingBadge(tx, ty, 24, 24, 20)) {
                    while (leerTouch(tx, ty)) delay(10); 
                    currentState = STATE_POPUP_UPDATE;
                    UIManager::drawUpdatePopup(gfx, "NUEVA VERSION", latestVersion.c_str());
                    popupLaunchTime = millis();
                }
            }
            break;
        }

        case STATE_POPUP_UPDATE: {
            int click = leerClickPopupUpdate();
            if (click == 1) { 
                if (game) { delete game; game = nullptr; }
                SceneManager::getInstance().changeScene(nullptr);
                AssetManager::getInstance().clearUnused();

                UpdateManager::Config updateCfg = {
                    .gfx = gfx, .githubUser = "OffShur3", .githubRepo = "Tamagotchi_LOVE",
                    .currentVersionPath = "/config/version.txt", .checkIntervalMs = 300000
                };
                
                UpdateManager* updateManager = new UpdateManager(updateCfg);
                updateManager->setLatestVersion(latestVersion);
                updateManager->performFullUpdate(); 
                delete updateManager;
            }
            else if (click == 2) { 
                updateAvailable = false;
                WiFi.disconnect();
                WiFi.mode(WIFI_OFF);
                currentState = STATE_GAMEPLAY;
                if (game) game->redraw();
            }
            break;
        }

        case STATE_POPUP: {
            int seleccion = leerClickPopup();
            if (seleccion == 1) { 
                currentState = STATE_PORTAL;
                if (game) { delete game; game = nullptr; }
                SceneManager::getInstance().changeScene(nullptr);
                AssetManager::getInstance().clearUnused();

                dibujarPantallaPortal();
                
                TamaNetworkManager::Config netCfg = {
                    .gfx = gfx,
                    .png = &png,
                    .qrPath = "/QR Network.png",
                    .jsonPath = "/config/wifi.json",
                    .apSSID = "TamaConfig",
                    .apPassword = "iluvUiluvU<3",
                    .pngOpen = pngOpen,
                    .pngClose = pngClose,
                    .pngRead = pngRead,
                    .pngSeek = pngSeek,
                    .pngDraw = qrDrawCallback,
                    .checkExit = checkExitCallback
                };
                TamaNetworkManager* netManager = new TamaNetworkManager(netCfg);
                netManager->begin();
                bool completado = netManager->runCaptivePortal();

                delete netManager;
                ESP.restart(); 
            }
            else if (seleccion == 2) { 
                Serial.println("[KERNEL] Seleccionado modo Offline. Apagando antena WiFi...");
                WiFi.disconnect();
                WiFi.mode(WIFI_OFF);
                currentState = STATE_GAMEPLAY;
            }
            break;
        }

        case STATE_PORTAL: break;
    }

    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase(); 

        if (cmd == "STATS" || cmd == "STATUS" || cmd == "PET" || cmd == "TAMA" || cmd == "INFO") {
            if (game) game->printStats();
        }
        else if (cmd == "DEBUG") {
            bool nextState = false;
            SD_MMC.mkdir("/config");
            if (SD_MMC.exists("/config/debug.txt")) {
                SD_MMC.remove("/config/debug.txt");
                nextState = false;
            } else {
                File f = SD_MMC.open("/config/debug.txt", "w");
                if (f) {
                    f.println("DEBUG_ACTIVE=ON");
                    f.close();
                }
                nextState = true;
            }
            Serial.printf("\n[KERNEL] Cambiando debug en SD a: %s. Reiniciando...\n", nextState ? "ACTIVADO" : "DESACTIVADO");
            delay(500);
            ESP.restart(); 
        }
        else if (cmd == "WIFI") {
            if (WiFi.getMode() != WIFI_OFF) {
                WiFi.disconnect();
                WiFi.mode(WIFI_OFF);
                Serial.println("\n[KERNEL] Comando recibido: Apagando antena WiFi.");
            }
        }
        else if (cmd == "RESET") {
            if (game) game->resetGame();
        }
    }

    delay(20);
}
