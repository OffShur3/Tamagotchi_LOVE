// src/main.cpp
#include <Arduino.h>
#include <SPI.h>
#include <SD_MMC.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <PNGdec.h>
#include <WiFiMulti.h> 
#include <ArduinoJson.h> 
#include <esp_sntp.h> 
#include <algorithm>
#include "Game.h"
#include "assets/AssetManager.h"
#include "render/SceneManager.h"
#include "core/TamaNetworkManager.h" 
#include "core/UpdateManager.h"
#include "core/touch_axs5106.h"
#include "tamagotchi/ClockWidget.h"
#include "tamagotchi/MessageManager.h"
#include "UI.h"
#include "ColorTuner.h"

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
    STATE_CONNECTING,
    STATE_GAMEPLAY,
    STATE_POPUP,
    STATE_PORTAL,
    STATE_POPUP_UPDATE,
    STATE_POPUP_DEATH
};

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
int leerClickPopupDeath();
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

bool deathPopupDeclined = false;

volatile bool peticionDormir = false;
volatile bool ntpSyncPending = false;

void cbNtpSync(struct timeval *tv) {
    time_t realNow = tv->tv_sec;
    struct tm tInfo;
    localtime_r(&realNow, &tInfo);

    Serial.printf("\n[TIME] ¡EVENTO NTP! Hora real recibida de Internet: [%02d:%02d:%02d]\n", 
                  tInfo.tm_hour, tInfo.tm_min, tInfo.tm_sec);
    
    ClockWidget::isSyncedWithInternet = true;
    ntpSyncPending = true;
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
                    while (leerTouch(curX, curY)) delay(10);
                    return true;
                }
            }
        }
    }
    return false;
}

bool checkExitCallback() { return detectarSwipeRight(); }

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

int leerClickPopupDeath() {
    if (millis() - popupLaunchTime < 500) return 0; 
    uint16_t tx, ty;
    if (leerTouch(tx, ty)) {
        int click = UIManager::getDeathPopupClick(tx, ty);
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

    struct NetCandidate {
        String ssid;
        String pass;
        int32_t rssi;
        bool visible;
    };

    std::vector<NetCandidate> candidatas;
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        const char* ssid = obj["ssid"];
        const char* pass = obj["pass"];
        if (ssid && strlen(ssid) > 0) {
            candidatas.push_back({String(ssid), String(pass ? pass : ""), -999, false});
        }
    }

    if (candidatas.empty()) return false;

    Serial.println("[KERNEL] Escaneando redes WiFi disponibles en el entorno...");
    int nRedesAire = WiFi.scanNetworks();
    Serial.printf("[KERNEL] Redes detectadas en el aire: %d\n", nRedesAire);

    std::vector<NetCandidate> visibles;
    std::vector<NetCandidate> noVisibles;

    for (int i = 0; i < nRedesAire; ++i) {
        String ssidAire = WiFi.SSID(i);
        int32_t rssiAire = WiFi.RSSI(i);

        for (auto& cand : candidatas) {
            if (cand.ssid == ssidAire) {
                if (!cand.visible || rssiAire > cand.rssi) {
                    cand.rssi = rssiAire;
                    cand.visible = true;
                }
            }
        }
    }

    for (const auto& cand : candidatas) {
        if (cand.visible) visibles.push_back(cand);
        else noVisibles.push_back(cand);
    }

    std::sort(visibles.begin(), visibles.end(), [](const NetCandidate& a, const NetCandidate& b) {
        return a.rssi > b.rssi;
    });

    redesGuardadas.clear();

    if (!visibles.empty()) {
        for (const auto& cand : visibles) {
            redesGuardadas.push_back({cand.ssid, cand.pass});
            Serial.printf("[KERNEL] Red priorizada en el aire: %-16s | Señal: %3d dBm\n", cand.ssid.c_str(), (int)cand.rssi);
        }
    } else {
        Serial.println("[KERNEL] Ninguna red conocida fue vista. Encolando todas como fallback...");
        for (const auto& cand : noVisibles) {
            redesGuardadas.push_back({cand.ssid, cand.pass});
        }
    }

    WiFi.scanDelete();
    return !redesGuardadas.empty();
}

void dibujarPopupStardew() { 
    UIManager::drawStardewPopup(gfx); 
}

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

    gfx->fillScreen(RETRO_BG);
    gfx->setTextColor(RETRO_WHITE);
    gfx->setTextSize(2);
    imprimirCentrado(gfx, "This is 4", 130, 2, RETRO_WHITE);
    imprimirCentrado(gfx, "u babe...", 160, 2, RETRO_WHITE);
    delay(1500);

    esperarSD();

    loadColorConfigSD(bus);

    Preferences prefs;
    prefs.begin("tama-kernel", false); 
    debugMode = prefs.getBool("debug", false);
    prefs.end();

    if (SD_MMC.exists("/config/debug.txt")) {
        debugMode = true;
        Serial.println("[KERNEL] Archivo /config/debug.txt detectado en la SD. MODO DEBUG ACTIVADO.");
    } else {
        debugMode = false;
    }

    AssetManager::getInstance().setPNG(&png);
    AssetManager::getInstance().setFileSystem(&SD_MMC); 
    MessageManager::getInstance().init();

    sntp_set_time_sync_notification_cb(cbNtpSync);

    game = new Game(172, 320);
    game->init();

    game->tick();
    game->flush(gfx);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    bool tieneRedes = cargarRedesSD();

    if (tieneRedes) {
        intentoRedActual = 0;
        Serial.printf("[KERNEL] Conectando directamente a: %s...\n", redesGuardadas[0].first.c_str());
        WiFi.begin(redesGuardadas[0].first.c_str(), redesGuardadas[0].second.c_str());
        
        connectionStartTime = millis();
        ultimoIntentoWiFi = millis();
        currentState = STATE_CONNECTING;
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

    if (ntpSyncPending) {
        ntpSyncPending = false;
        if (game) game->onTimeSynced(); 
    }

    if (game && (currentState == STATE_GAMEPLAY || currentState == STATE_CONNECTING)) {
        game->tick();

        if (updateAvailable && !updateInProgress && currentState == STATE_GAMEPLAY) {
            UIManager::drawUpdateBadgeFB(game->getFramebuffer(), 172, 320, 24, 24, 12); 
        }

        game->flush(gfx);
    }

    switch (currentState) {
        case STATE_CONNECTING: {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("\n[KERNEL] ¡WiFi Conectado con éxito a: %s!\n", redesGuardadas[intentoRedActual].first.c_str());
                
                setenv("TZ", "ART3", 1);
                tzset();

                configTzTime("ART3", "time.google.com", "pool.ntp.org", "time.nist.gov");
                Serial.println("[TIME] Solicitando hora real a servidores NTP...");

                currentState = STATE_GAMEPLAY;
                break;
            } else {
                if (millis() - ultimoIntentoWiFi > 12000) {
                    intentoRedActual++;
                    if (intentoRedActual < (int)redesGuardadas.size()) {
                        Serial.printf("[KERNEL] Probando siguiente red en cola: %s...\n", redesGuardadas[intentoRedActual].first.c_str());
                        WiFi.disconnect();
                        WiFi.begin(redesGuardadas[intentoRedActual].first.c_str(), redesGuardadas[intentoRedActual].second.c_str());
                        ultimoIntentoWiFi = millis();
                    } else {
                        intentoRedActual = 0; 
                        WiFi.disconnect();
                        WiFi.begin(redesGuardadas[0].first.c_str(), redesGuardadas[0].second.c_str());
                        ultimoIntentoWiFi = millis();
                    }
                }

                uint32_t tiempoLimiteTotal = ((uint32_t)redesGuardadas.size() * 13000) + 5000;
                if (millis() - connectionStartTime > tiempoLimiteTotal) { 
                    Serial.println("[KERNEL] Timeout de conexión excedido. Pasando a Offline.");
                    WiFi.disconnect();
                    WiFi.mode(WIFI_OFF);
                    currentState = STATE_POPUP;
                    UIManager::drawStardewPopup(gfx);
                    popupLaunchTime = millis();
                }
            }
            break;
        }

        case STATE_GAMEPLAY: {
            if (game && game->isPetDead() && !deathPopupDeclined) {
                if (game->hasDeathTouchOccurred()) {
                    game->clearDeathTouch();
                    Serial.println("[KERNEL] Toque en mascota fallecida detectado. Mostrando Popup de reinicio...");
                    currentState = STATE_POPUP_DEATH;
                    UIManager::drawDeathPopup(gfx);
                    popupLaunchTime = millis();
                    break;
                }
            }

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
                    Serial.println("[MAIN] Version Identica. Iniciando precarga de frases por IA...");
                    MessageManager::getInstance().startPreload();
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

        case STATE_POPUP_DEATH: {
            int click = leerClickPopupDeath();
            if (click == 1) { 
                Serial.println("[KERNEL] Reiniciando partida tras muerte...");
                if (game) game->resetGame();
                deathPopupDeclined = false;
                currentState = STATE_GAMEPLAY;
            } else if (click == 2) { 
                Serial.println("[KERNEL] Reinicio cancelado. La mascota permanecerá muerta en esta sesión.");
                deathPopupDeclined = true; 
                currentState = STATE_GAMEPLAY;
                if (game) game->redraw();
            }
            break;
        }

        case STATE_POPUP_UPDATE: {
            int click = leerClickPopupUpdate();
            if (click == 1) { 
                Serial.println("[MAIN] Iniciando actualizacion completa. Desalojando RAM...");
                
                if (game) {
                    delete game;
                    game = nullptr;
                }
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
                Serial.println("[KERNEL] Cargando Portal Cautivo...");
                currentState = STATE_PORTAL;
                
                if (game) {
                    delete game; 
                    game = nullptr;
                }
                SceneManager::getInstance().changeScene(nullptr);
                AssetManager::getInstance().clearUnused();

                dibujarPantallaPortal();
                
                TamaNetworkManager::Config netCfg = {
                    .gfx = gfx, .png = &png, .qrPath = "/QR Network.png",
                    .jsonPath = "/config/wifi.json", .apSSID = "TamaConfig",
                    .apPassword = "iluvUiluvU<3", .pngOpen = pngOpen,
                    .pngClose = pngClose, .pngRead = pngRead,
                    .pngSeek = pngSeek, .pngDraw = qrDrawCallback,
                    .checkExit = checkExitCallback
                };
                TamaNetworkManager* netManager = new TamaNetworkManager(netCfg);
                netManager->begin();
                bool completado = netManager->runCaptivePortal();

                if (!completado) {
                    delete netManager;
                    delay(500);
                    ESP.restart(); 
                } else {
                    delete netManager;
                    ESP.restart();
                }
            }
            else if (seleccion == 2) { 
                Serial.println("[KERNEL] Seleccionado modo Offline. Continuando...");
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

        if (cmd == "HELP") {
            Serial.println("\n==================================================");
            Serial.println("         TAMA KERNEL - SUITE DE COMANDOS          ");
            Serial.println("==================================================");
            Serial.println("  EVOLVE         : Forzar evolucion a siguiente etapa");
            Serial.println("  STAGE <nom>    : Saltar a etapa (EGG, BABY, CHILD, ADULT, SENIOR)");
            Serial.println("  ANIMS / TEST   : Showcase de todas las animaciones en SD");
            Serial.println("  KILL / DIE     : Forzar defuncion (etapa actual + dead.png)");
            Serial.println("  REVIVE         : Resucitar mascota al 100% de salud");
            Serial.println("  GODMODE        : 100% a todas las estadisticas y limpiar");
            Serial.println("  STARVE         : Hambre a 0% (hambre critica/llanto)");
            Serial.println("  BORED          : Felicidad a 10% (aburrimiento)");
            Serial.println("  HAPPY          : Felicidad al 100% + dopamina");
            Serial.println("  SICK           : Inducir enfermedad y dolor (salud baja)");
            Serial.println("  HEAL           : Curar con botiquin al 100%");
            Serial.println("  POOP           : Forzar 1 caca (+1)");
            Serial.println("  POOPMAX        : Forzar 3 cacas al instante");
            Serial.println("  CLEAN          : Limpiar suelo");
            Serial.println("  TIRED          : Drenar energia a 0%");
            Serial.println("  FEED / PET     : Alimentar / Acariciar");
            Serial.println("  LIGHTS         : Alternar luz encendida/apagada");
            Serial.println("  STATS / INFO   : Diagnostico completo del sistema");
            Serial.println("  DEBUG          : Alternar visualizacion debug en SD");
            Serial.println("  STIM <0..8> <V>: Estimular neurona con impulso gradual");
            Serial.println("  THOUGHT <tipo> : Nudge a pensamiento (HEART, FOOD, NOSTALGIA...)");
            Serial.println("  POPUP <tipo>   : Forzar popup (DEATH, UPDATE, NET)");
            Serial.println("  AI <emocion>   : Disparar dialogo IA (HAPPY, SICK, DEAD...)");
            Serial.println("  RESET          : Reiniciar partida");
            Serial.println("  TUNECOLOURS   : Iniciar calibrador de color (Solo modo Debug)");
            Serial.println("==================================================");
        }
        else if (cmd == "EVOLVE") {
            if (game) game->petEvolveNext();
        }
        else if (cmd.startsWith("STAGE ")) {
            String s = cmd.substring(6); s.trim();
            if (s == "EGG") game->petJumpStage(PetStage::Egg);
            else if (s == "BABY") game->petJumpStage(PetStage::Baby);
            else if (s == "CHILD") game->petJumpStage(PetStage::Child);
            else if (s == "ADULT") game->petJumpStage(PetStage::Adult);
            else if (s == "SENIOR") game->petJumpStage(PetStage::Senior);
        }
        else if (cmd == "ANIMS" || cmd == "TEST" || cmd == "SHOWCASE") {
            if (game) game->startAnimationTest();
        }
        else if (cmd == "KILL" || cmd == "DIE") {
            if (game) game->forcePetState(PetState::Dead);
        }
        else if (cmd == "REVIVE") {
            if (game) game->petRevive();
        }
        else if (cmd == "GODMODE") {
            if (game) game->petGodMode();
        }
        else if (cmd == "STARVE") {
            if (game) game->petStarve();
        }
        else if (cmd == "BORED") {
            if (game) game->petMakeBored();
        }
        else if (cmd == "HAPPY") {
            if (game) game->petMakeHappy();
        }
        else if (cmd == "SICK") {
            if (game) game->petMakeSick();
        }
        else if (cmd == "HEAL") {
            if (game) game->petHeal();
        }
        else if (cmd == "POOP") {
            if (game) game->petForcePoop();
        }
        else if (cmd == "POOPMAX") {
            if (game) game->petMaxPoop();
        }
        else if (cmd == "CLEAN") {
            if (game) game->petClean();
        }
        else if (cmd == "TIRED") {
            if (game) game->petDrainEnergy();
        }
        else if (cmd == "FEED") {
            if (game) game->petFeed();
        }
        else if (cmd == "PET") {
            if (game) game->petPet();
        }
        else if (cmd == "LIGHTS") {
            if (game) game->petToggleLights();
        }
        else if (cmd == "STATS" || cmd == "INFO") {
            if (game) game->printStats();
        }
        else if (cmd == "PLAY" || cmd == "GAME") {
            if (game) game->startMinigame();
        }
        else if (cmd == "RESET") {
            if (game) game->resetGame();
        }
        else if (cmd == "DEBUG") {
            SD_MMC.mkdir("/config");
            if (SD_MMC.exists("/config/debug.txt")) {
                SD_MMC.remove("/config/debug.txt");
                Serial.println("[KERNEL] Debug desactivado en SD. Reiniciando...");
            } else {
                File f = SD_MMC.open("/config/debug.txt", "w");
                if (f) { f.println("ON"); f.close(); }
                Serial.println("[KERNEL] Debug activado en SD. Reiniciando...");
            }
            delay(500);
            ESP.restart();
        }
        else if (cmd == "TUNECOLOURS") {
            if (!debugMode) {
                Serial.println("[KERNEL] Modo debug inactivo. Primero envía el comando 'DEBUG' o crea /config/debug.txt en la SD.");
            } else {
                Serial.println("[KERNEL] Entrando al módulo ColorTuner...");
                runColorTuner(gfx, bus, &png);
            }
        }
        else if (cmd.startsWith("STIM ")) {
            int firstSpace = cmd.indexOf(' ');
            int secondSpace = cmd.indexOf(' ', firstSpace + 1);
            if (firstSpace != -1 && secondSpace != -1) {
                int nIdx = cmd.substring(firstSpace + 1, secondSpace).toInt();
                float val = cmd.substring(secondSpace + 1).toFloat();
                if (game) game->stimulateBrain(nIdx, val);
                Serial.printf("[TEST] Estimulando neurona %d con impulso: %.2f\n", nIdx, val);
            }
        }
        else if (cmd.startsWith("THOUGHT ")) {
            String tStr = cmd.substring(8); tStr.trim();
            ThoughtType tt = THOUGHT_HEART;
            if (tStr == "FOOD") tt = THOUGHT_FOOD;
            else if (tStr == "PLAY") tt = THOUGHT_PLAY;
            else if (tStr == "SLEEP") tt = THOUGHT_SLEEP;
            else if (tStr == "POOP") tt = THOUGHT_POOP;
            else if (tStr == "MED") tt = THOUGHT_MED;
            else if (tStr == "STRESS") tt = THOUGHT_STRESS;
            else if (tStr == "CURIOUS") tt = THOUGHT_CURIOUS;
            else if (tStr == "NOSTALGIA" || tStr == "DAYDREAM") tt = THOUGHT_NOSTALGIA;
            else if (tStr == "ANTICIPATION" || tStr == "WAIT") tt = THOUGHT_ANTICIPATION;
            else if (tStr == "CAPRICE" || tStr == "BOREDOM") tt = THOUGHT_CAPRICE;
            else if (tStr == "NONE") tt = THOUGHT_NONE;
            
            if (game) game->nudgeThought(tt, "Estimulo incremental", 0.35f);
            Serial.printf("[TEST] Nudge de pensamiento: %s (+acumulacion)\n", tStr.c_str());
        }
        else if (cmd.startsWith("POPUP ")) {
            String pop = cmd.substring(6); pop.trim();
            if (pop == "DEATH") {
                currentState = STATE_POPUP_DEATH;
                UIManager::drawDeathPopup(gfx);
                popupLaunchTime = millis();
            } else if (pop == "UPDATE") {
                currentState = STATE_POPUP_UPDATE;
                UIManager::drawUpdatePopup(gfx, "NUEVA VERSION", "v1.0.0");
                popupLaunchTime = millis();
            } else if (pop == "NET") {
                currentState = STATE_POPUP;
                UIManager::drawStardewPopup(gfx);
                popupLaunchTime = millis();
            }
        }
        else if (cmd.startsWith("AI ")) {
            String mood = cmd.substring(3); mood.trim(); mood.toLowerCase();
            MessageManager::getInstance().requestAiMessage(mood, "Comando manual forzado desde Serial");
        }
        else if (cmd.startsWith("STATE ")) {
            String target = cmd.substring(6); target.trim();
            if (target == "IDLE") game->forcePetState(PetState::Idle);
            else if (target == "HAPPY") game->forcePetState(PetState::Happy);
            else if (target == "EATING") game->forcePetState(PetState::Eating);
            else if (target == "SLEEPING") game->forcePetState(PetState::Sleeping);
            else if (target == "SICK") game->forcePetState(PetState::Sick);
            else if (target == "SAD") game->forcePetState(PetState::Sad);
            else if (target == "DEAD") game->forcePetState(PetState::Dead);
        }
    }

    delay(20);
}
