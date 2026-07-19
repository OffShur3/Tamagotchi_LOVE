// src/main.cpp
#include <Arduino.h>
#include <SPI.h>
#include <SD_MMC.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <PNGdec.h>
#include <WiFiMulti.h> 
#include <ArduinoJson.h> 
#include "Game.h"
#include "assets/AssetManager.h"
#include "render/SceneManager.h"
#include "core/TamaNetworkManager.h" 
#include "core/UpdateManager.h"
#include "core/touch_axs5106.h"
#include "UI.h"

// --- Pines físicos reales de Waveshare ESP32-S3 Touch LCD 1.47" ---
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

// Constantes de color físicas para la intro de bienvenida
#define RETRO_BG    0x1042 // MAT_BG
#define RETRO_WHITE 0xFFFF // BGR_WHITE

enum KernelState {
    STATE_SCANNING,
    STATE_CONNECTING,
    STATE_GAMEPLAY,
    STATE_POPUP,
    STATE_PORTAL,
    STATE_POPUP_UPDATE
};

// --- DECLARACIONES AVANZADAS (Prototipos de función para el Compilador) ---
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

// Inicializar bus SPI de pantalla
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 0, false, 172, 320, 34, 0, 34, 0);

// Puntero dinámico a la clase base de juego
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

// Callback autónomo para dibujar el QR de red centrado en la pantalla física (Stack-Safe)
int qrDrawCallback(PNGDRAW *pDraw) {
    png.getLineAsRGB565(pDraw, globalLineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0);
    int y = pDraw->y + 25; 
    int x_offset = (172 - pDraw->iWidth) / 2; 
    gfx->draw16bitRGBBitmap(x_offset, y, globalLineBuffer, pDraw->iWidth, 1);
    return 1;
}

// Lector directo de pantalla táctil AXS5106 con mapeo de resolución
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

// Detección de Swipe de Izquierda a Derecha (Swipe Right) para salir del portal
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

// Filtro táctil contra toques fantasma de calibración de arranque en el popup
int leerClickPopup() {
    if (millis() - popupLaunchTime < 500) {
        return 0; 
    }
    uint16_t tx, ty;
    if (leerTouch(tx, ty)) {
        int click = UIManager::getStardewPopupClick(tx, ty);
        if (click > 0) {
            unsigned long pressTime = millis();
            while (leerTouch(tx, ty) && (millis() - pressTime < 2000)) { 
                delay(10); 
            }
            return click;
        }
    }
    return 0;
}

// Filtro táctil para el menú de actualización
int leerClickPopupUpdate() {
    if (millis() - popupLaunchTime < 500) {
        return 0; 
    }
    uint16_t tx, ty;
    if (leerTouch(tx, ty)) {
        int click = UIManager::getUpdatePopupClick(tx, ty);
        if (click > 0) {
            unsigned long pressTime = millis();
            while (leerTouch(tx, ty) && (millis() - pressTime < 2000)) { 
                delay(10); 
            }
            return click;
        }
    }
    return 0;
}

// Esperar a que la tarjeta SD sea insertada físicamente (Hardware Safety)
bool esperarSD() {
    gfx->fillScreen(RETRO_BG);
    imprimirCentrado(gfx, "SD", 130, 2, RETRO_WHITE);
    imprimirCentrado(gfx, "no detectada", 160, 2, RETRO_WHITE);
    imprimirCentrado(gfx, "Por favor insertela", 190, 1, RETRO_WHITE);
       
    while (true) {
        SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
        if (SD_MMC.begin("/sdcard", true)) {
            return true;
        }
        delay(500);
    }
}

// Guardado persistente y apagado total de hardware (Deep Sleep)
void irADormir() {
    Serial.println("[POWER] Guardando y entrando en Deep Sleep...");
    
    // CORRECCIÓN: Guardar partida directamente sobre el puntero base Game*
    if (game) {
        game->saveGame(); 
    }

    digitalWrite(TFT_BL, LOW);
    gfx->displayOff();

    esp_sleep_enable_ext0_wakeup((gpio_num_t)BOOT_PIN, 0); 
    
    esp_deep_sleep_start();
}

bool cargarRedesSD() {
    if (!SD_MMC.exists("/tama/config/wifi.json")) return false;

    File file = SD_MMC.open("/tama/config/wifi.json", "r");
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

// Renderizador del Popup de Stardew Valley en pantalla (Deadlock-Free)
void dibujarPopupStardew() {
    UIManager::drawStardewPopup(gfx);
}

// Pintar la pantalla de instrucciones del Portal Cautivo polida (Deadlock-Free)
void dibujarPantallaPortal() {
    gfx->fillScreen(0x1042); // MAT_BG

    if (!SD_MMC.exists("/QR Network.png")) {
        Serial.println("[PORTAL] ERROR: /QR Network.png no encontrado en la SD");
        imprimirCentrado(gfx, "QR no encontrado", 50, 1, 0xF800);
        imprimirCentrado(gfx, "Revisa la SD", 70, 1, 0xF800);
    } else {
        int rc = png.open("/QR Network.png", pngOpen, pngClose, pngRead, pngSeek, qrDrawCallback);
        if (rc == PNG_SUCCESS) {
            png.decode(NULL, 0);
            png.close();
            Serial.println("[PORTAL] QR decodificado con exito");
        } else {
            imprimirCentrado(gfx, "Error decodificador", 50, 1, 0xF800);
        }
    }

    // Textos informativos estilizados
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

    // Inicialización del digitalizador táctil
    touch_init();

    // Configurar interrupción física de ahorro de energía (Deep Sleep)
    pinMode(TFT_BL, OUTPUT);
    pinMode(BOOT_PIN, INPUT_PULLUP); 
    attachInterrupt(digitalPinToInterrupt(BOOT_PIN), isrBotonBoot, FALLING);
    digitalWrite(TFT_BL, HIGH);
    
    if (!gfx->begin()) {
        Serial.println("Error inicializando pantalla física.");
    }
    
    bus->beginWrite();
    bus->writeCommand(0x36);
    bus->write(0x48); 
    bus->endWrite();

    // --- PANTALLA DE INTRO BIENVENIDA ---
    gfx->fillScreen(RETRO_BG);
    gfx->setTextColor(RETRO_WHITE);
    gfx->setTextSize(2);
    imprimirCentrado(gfx, "This is 4", 130, 2, RETRO_WHITE);
    imprimirCentrado(gfx, "u babe...", 160, 2, RETRO_WHITE);
    delay(1500);

    esperarSD();

    // --- BANNER DE INICIO ---
    String versionArranque = "v0.0.0";
    if (SD_MMC.exists("/version.txt")) {
        File vFile = SD_MMC.open("/version.txt", "r");
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
    // --------------------------------

    Preferences prefs;
    prefs.begin("tama-kernel", false);
    debugMode = prefs.getBool("debug", false);
    prefs.end();

    if (SD_MMC.exists("/DEBUG.txt")) {
        debugMode = true;
        Serial.println("[KERNEL] Archivo /DEBUG.txt detectado en la SD. MODO DEBUG ACTIVADO.");
    } else {
        debugMode = false;
    }

    AssetManager::getInstance().setPNG(&png);
    AssetManager::getInstance().setFileSystem(&SD_MMC); 

    // 2. CREACIÓN DINÁMICA DEL JUEGO
    game = new Game(172, 320);
    game->init();

    // Enviar el primer frame a pantalla para una transición ultra-fluida antes de encender WiFi
    game->tick();
    game->flush(gfx);

    // 3. CARGA DE REDES MULTI-WIFI
    Serial.println("[KERNEL] Iniciando escaneo de redes en background...");
    WiFi.mode(WIFI_STA);
    
    bool tieneRedes = cargarRedesSD();

    if (tieneRedes) {
        // Escaneo ASÍNCRONO (el 'true' evita que la animación se congele)
        WiFi.scanNetworks(true); 
        connectionStartTime = millis();
        currentState = STATE_SCANNING;
    } else {
        Serial.println("[KERNEL] No se detectaron redes validas. Saltando al Popup.");
        WiFi.mode(WIFI_OFF);
        currentState = STATE_POPUP;
        delay(100); 
        UIManager::drawStardewPopup(gfx);
        popupLaunchTime = millis();
    }
}

void loop() {
    // 1. Revisar solicitud de interrupción de suspensión (Deep Sleep)
    if (peticionDormir) {
        peticionDormir = false; 
        irADormir();
    }

    // --- TIME SLICING: SOLO RENDERIZAR SI ESTAMOS EN GAMEPLAY, SCANNING O CONECTANDO ---
    if (game && (currentState == STATE_GAMEPLAY || currentState == STATE_SCANNING || currentState == STATE_CONNECTING)) {
        game->tick();

        if (updateAvailable && !updateInProgress && currentState == STATE_GAMEPLAY) {
            UIManager::drawUpdateBadgeFB(game->getFramebuffer(), 172, 320, 24, 24, 12); 
        }

        game->flush(gfx);
    }

    // --- MÁQUINA DE ESTADOS DEL KERNEL ---
    switch (currentState) {
        case STATE_SCANNING: {
            int n = WiFi.scanComplete();
            if (n >= 0) {
                // Escaneo terminado en segundo plano
                std::vector<std::pair<String, String>> redesDisponibles;
                
                for (int i = 0; i < n; ++i) {
                    String ssid = WiFi.SSID(i);
                    for (const auto& red : redesGuardadas) {
                        if (red.first == ssid) {
                            redesDisponibles.push_back(red);
                            break; // Encontramos coincidencia
                        }
                    }
                }
                WiFi.scanDelete(); // Liberar RAM del escaneo

                if (!redesDisponibles.empty()) {
                    redesGuardadas = redesDisponibles; // Nos quedamos SOLO con las que existen físicamente cerca
                    Serial.printf("[KERNEL] Red al alcance: %s. Conectando...\n", redesGuardadas[0].first.c_str());
                    WiFi.begin(redesGuardadas[0].first.c_str(), redesGuardadas[0].second.c_str());
                    ultimoIntentoWiFi = millis();
                    intentoRedActual = 0;
                    currentState = STATE_CONNECTING;
                } else {
                    Serial.println("[KERNEL] Ninguna red guardada está al alcance. Pasando a Offline.");
                    WiFi.mode(WIFI_OFF);
                    currentState = STATE_POPUP;
                    UIManager::drawStardewPopup(gfx);
                    popupLaunchTime = millis();
                }
            } else if (n == WIFI_SCAN_FAILED) {
                Serial.println("[KERNEL] Falló el escaneo. Reintentando...");
                WiFi.scanNetworks(true);
            }

            // Timeout de seguridad
            if (millis() - connectionStartTime > 15000) {
                Serial.println("[KERNEL] Timeout de escaneo excedido. Lanzando Popup.");
                WiFi.mode(WIFI_OFF);
                currentState = STATE_POPUP;
                UIManager::drawStardewPopup(gfx);
                popupLaunchTime = millis();
            }
            break;
        }

        case STATE_CONNECTING: {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[KERNEL] WiFi conectado con exito.");
                currentState = STATE_GAMEPLAY;
            } else {
                // Si pasaron 5 segundos y no conectó, intentar con la siguiente red de la lista FILTRADA
                if (millis() - ultimoIntentoWiFi > 5000) {
                    intentoRedActual++;
                    if (intentoRedActual < redesGuardadas.size()) {
                        Serial.printf("[KERNEL] Intentando con red: %s\n", redesGuardadas[intentoRedActual].first.c_str());
                        WiFi.disconnect();
                        WiFi.begin(redesGuardadas[intentoRedActual].first.c_str(), redesGuardadas[intentoRedActual].second.c_str());
                        ultimoIntentoWiFi = millis();
                    } else {
                        intentoRedActual = 0; // Volver a empezar la lista
                        ultimoIntentoWiFi = millis();
                    }
                }
            }

            if (millis() - connectionStartTime > 20000) { // 20s total (escaneo + conexion)
                Serial.println("[KERNEL] Timeout de WiFi excedido. Lanzando Popup interactivo.");
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
                    .currentVersionPath = "/version.txt",
                    .checkIntervalMs = 300000 
                };
                UpdateManager updateManager(updateCfg);

                Serial.println("[MAIN] Ecosistema Wifi Online. Testeando Estado y Servidor OTA Seguro.");
                
                // Hace uso 100% resistente 
                if (updateManager.checkForUpdate()) {
                    updateAvailable = true;
                    latestVersion = updateManager.getLatestVersion();
                    
                    if (updateManager.needMandatoryUpdate()) {
                        Serial.println("[MAIN] Peticion Update Mandatorio Confirmado vía Web - Interfaz Abierta...");
                        mandatoryUpdate = true;
                    } 
                } 
                else if (updateManager.getLatestVersion() == updateManager.getCurrentVersion() 
                         && updateManager.getCurrentVersion() != "v0.0.0") 
                {
                     // ----- ÚNICO CONDICIONAL VALIDO Y DEMOSTRADO PARA "OFFLINE O DESACTVIO RADAR WI FI"
                    Serial.println("[MAIN] Confirmación NUBE V. Identica. Sistema Inicia Fase Offline, Extinguiendo Radar");
                    WiFi.disconnect(true);
                    WiFi.mode(WIFI_OFF);
                } 
                else 
                {
                     // Fue interrumpido su test. Que vuelva intentar tras rato largo porque dió cortes local red!
                    Serial.println("[MAIN] Ocurrio desvanecimiento Peticiones.. Postergando chequeos.");
                    WiFi.disconnect(true);
                    WiFi.mode(WIFI_OFF);
                }
            }

            // Ya evitamos llamar chequeos fantasma que tiran delay el bucle periodico. Al matar wifi esto libera para juegos fluidos todo!. 

            if (updateAvailable && !updateInProgress) {
                // UI Mantiene
                uint16_t tx, ty;
                if (leerTouch(tx, ty) && UIManager::isTouchingBadge(tx, ty, 24, 24, 20)) {
                    while (leerTouch(tx, ty)) { delay(10); } 
                    
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
                Serial.println("[MAIN] Decodificadora aceptada Actualización completa vía OTA...");
                if (game) { delete game; game = nullptr; }
                SceneManager::getInstance().changeScene(nullptr);
                AssetManager::getInstance().clearUnused();

                UpdateManager::Config updateCfg = {
                    .gfx = gfx, .githubUser = "OffShur3", .githubRepo = "Tamagotchi_LOVE",
                    .currentVersionPath = "/version.txt", .checkIntervalMs = 300000
                };
                
                // MODO ANTI-DESBORDAMIENTO: Alojamos todo dinámicamente en el Heap!
                UpdateManager* updateManager = new UpdateManager(updateCfg);
                updateManager->setLatestVersion(latestVersion);
                updateManager->performFullUpdate(); 
                
                delete updateManager; // Limpiar al terminar
            }
            else if (click == 2) { 
                Serial.println("[MAIN] Usuario postergo o se asusto sobre bajar nuevo OS.");
                updateAvailable = false;
                
                Serial.println("[MAIN] ---> Apagando Modulo Antena tras Posponer Viaje Update, Limpiando cache interno Wlan de RAM.  <--- ");
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                
                currentState = STATE_GAMEPLAY;
                if (game) { game->redraw(); }
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
                    .gfx = gfx,
                    .png = &png,
                    .qrPath = "/QR Network.png",
                    .jsonPath = "/tama/config/wifi.json",
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

                if (!completado) {
                    Serial.println("[KERNEL] Configuración cancelada. Reiniciando de forma segura...");
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

        case STATE_PORTAL: {
            break;
        }
    }

    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase(); 

        if (cmd == "DEBUG") {
            bool nextState = false;
            if (SD_MMC.exists("/DEBUG.txt")) {
                SD_MMC.remove("/DEBUG.txt");
                nextState = false;
            } else {
                File f = SD_MMC.open("/DEBUG.txt", "w");
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
        // COMANDO WIFI
        else if (cmd == "WIFI") {
            if (WiFi.getMode() != WIFI_OFF) {
                Serial.println("\n[KERNEL] Comando recibido: Apagando antena WiFi (Modo Offline).");
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
            } else {
                Serial.println("\n[KERNEL] Comando recibido: El WiFi ya estaba apagado.");
            }
        }
        else if (cmd == "INFO" || cmd == "HEAP" || cmd == "RAM") {
            uint32_t totalSDSize = SD_MMC.totalBytes() / (1024 * 1024);
            uint32_t usedSDSize = SD_MMC.usedBytes() / (1024 * 1024);

            Serial.println("\n==================================================");
            Serial.println("         TAMA KERNEL SYSTEM DIAGNOSTICS           ");
            Serial.println("==================================================");
            Serial.printf("  Free Heap RAM:      %7u Bytes (%u KB)\n", ESP.getFreeHeap(), ESP.getFreeHeap() / 1024);
            Serial.printf("  Min Free Heap:      %7u Bytes (%u KB)\n", ESP.getMinFreeHeap(), ESP.getMinFreeHeap() / 1024);
            Serial.printf("  Bloque contiguo Max:%7u Bytes (%u KB)\n", 
                          heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
            Serial.printf("  PSRAM Detectada:    %7s\n", psramFound() ? "SI" : "NO");
            Serial.println("--------------------------------------------------");
            Serial.printf("  SD Card Total:      %7u MB\n", totalSDSize);
            Serial.printf("  SD Card Usada:      %7u MB\n", usedSDSize);
            Serial.println("==================================================");
        }
        else if (cmd == "RESET") {
            if (game) {
                game->resetGame();
            }
        }
    }

    delay(20);
}
