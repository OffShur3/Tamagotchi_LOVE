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

// Paleta de Colores de Interfaz (Stardew Valley Style)
#define TAMA_UI_BG  0xF6FA 
#define TAMA_BROWN  0x4903 
#define TAMA_GREEN  0x2444 
#define TAMA_DARK_S 0x10A2 

// Máquina de estados de conexión del Kernel
enum KernelState {
    STATE_CONNECTING,
    STATE_GAMEPLAY,
    STATE_POPUP,
    STATE_PORTAL
};

PNG png;
WiFiMulti wifiMulti; 

// Búfer de línea global estático de 512 píxeles
uint16_t globalLineBuffer[512];

// Inicializar bus SPI de pantalla
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 0, false, 172, 320, 34, 0, 34, 0);

// Puntero dinámico al juego en Heap
Game* game = nullptr;

bool debugMode = false;
KernelState currentState = STATE_CONNECTING;
uint32_t connectionStartTime = 0;
uint32_t popupLaunchTime = 0; 

// Callbacks requeridos para el renderizado del QR de red
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

// Detección de Swipe de Izquierda a Derecha (Swipe Right) para salir
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

// Inyección de función de salida para NetworkManager (Swipe de Izquierda a Derecha)
bool checkExitCallback() {
    return detectarSwipeRight();
}

void imprimirCentrado(const char* texto, int y, int size, uint16_t color) {
    gfx->setTextSize(size);
    gfx->setTextColor(color);
    int anchoPorChar = size * 6;
    int anchoTexto = strlen(texto) * anchoPorChar;
    int x = (172 - anchoTexto) / 2;
    gfx->setCursor(x, y);
    gfx->println(texto);
}

// Renderizador del Popup de Stardew Valley en pantalla (Deadlock-Free)
void dibujarPopupStardew() {
    gfx->fillRoundRect(14, 84, 148, 156, 12, TAMA_DARK_S);
    gfx->fillRoundRect(10, 80, 152, 160, 12, TAMA_BROWN); 
    gfx->fillRoundRect(13, 83, 146, 154, 10, TAMA_UI_BG);

    imprimirCentrado("CONEXION", 95, 1, TAMA_BROWN);
    imprimirCentrado("FALLIDA", 108, 1, TAMA_BROWN);

    // Botón Online (Verde)
    gfx->fillRoundRect(22, 130, 128, 35, 8, TAMA_BROWN); 
    gfx->fillRoundRect(24, 132, 124, 31, 6, TAMA_GREEN); 
    imprimirCentrado("Configurar", 142, 1, 0xFFFF);

    // Botón Offline (Marrón secundario)
    gfx->fillRoundRect(22, 180, 128, 35, 8, TAMA_BROWN); 
    gfx->fillRoundRect(24, 182, 124, 31, 6, 0x9306); 
    imprimirCentrado("Jugar Offline", 192, 1, 0xFFFF);
}

// Lector de toque sobre las zonas de los botones del Popup con timeout de seguridad
int leerClickPopup() {
    // Evitar cualquier lectura si no ha transcurrido medio segundo (Filtro de ruido en arranque)
    if (millis() - popupLaunchTime < 500) {
        return 0; 
    }

    uint16_t tx, ty;
    if (leerTouch(tx, ty)) {
        // Botón superior (y: 130-165, x: 22-150) -> Configurar (Online)
        if (ty >= 130 && ty <= 165 && tx >= 22 && tx <= 150) {
            unsigned long pressTime = millis();
            while (leerTouch(tx, ty) && (millis() - pressTime < 2000)) { 
                delay(10); 
            }
            return 1;
        }
        // Botón inferior (y: 180-215, x: 22-150) -> Jugar Offline
        if (ty >= 180 && ty <= 215 && tx >= 22 && tx <= 150) {
            unsigned long pressTime = millis();
            while (leerTouch(tx, ty) && (millis() - pressTime < 2000)) { 
                delay(10); 
            }
            return 2;
        }
    }
    return 0;
}

// Pintar la pantalla de instrucciones del Portal Cautivo polida (Deadlock-Free)
void dibujarPantallaPortal() {
    gfx->fillScreen(0x1042); // MAT_BG

    if (!SD_MMC.exists("/QR Network.png")) {
        Serial.println("[PORTAL] ERROR: /QR Network.png no encontrado en la SD");
        imprimirCentrado("QR no encontrado", 50, 1, 0xF800);
        imprimirCentrado("Revisa la SD", 70, 1, 0xF800);
    } else {
        int rc = png.open("/QR Network.png", pngOpen, pngClose, pngRead, pngSeek, qrDrawCallback);
        if (rc == PNG_SUCCESS) {
            png.decode(NULL, 0);
            png.close();
            Serial.println("[PORTAL] QR decodificado con exito");
        } else {
            imprimirCentrado("Error decodificador", 50, 1, 0xF800);
        }
    }

    // Textos informativos estilizados
    imprimirCentrado("Conecta tu celular a:", 135, 1, 0xFEE0);
    imprimirCentrado("TamaConfig", 150, 2, 0xFFFF);
    
    imprimirCentrado("Admite conexion", 185, 1, 0x07E0);
    imprimirCentrado("sin Internet", 200, 1, 0x07E0);
    
    imprimirCentrado("Entra al navegador", 230, 1, 0xFFFF);
    imprimirCentrado("y elige tu WiFi", 245, 1, 0xFFFF);
    
    imprimirCentrado("<3", 270, 2, 0xFFFF);
}

// --- CARGA DE REDES CON RETORNO DE ESTADO ---
bool cargarRedesSD() {
    if (!SD_MMC.exists("/tama/config/wifi.json")) {
        Serial.println("[KERNEL] No se encontro /tama/config/wifi.json en la SD.");
        return false;
    }

    File file = SD_MMC.open("/tama/config/wifi.json", "r");
    if (!file) return false;

    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.println("[KERNEL] Error decodificando el archivo wifi.json.");
        return false;
    }

    JsonArray arr = doc.as<JsonArray>();
    int redesRegistradas = 0;
    
    for (JsonObject obj : arr) {
        const char* ssid = obj["ssid"];
        const char* pass = obj["pass"];
        if (ssid && strlen(ssid) > 0) {
            wifiMulti.addAP(ssid, pass);
            Serial.printf("[KERNEL] Red registrada en Multi-WiFi: %s\n", ssid);
            redesRegistradas++;
        }
    }
    return (redesRegistradas > 0);
}

void setup() {
    Serial.begin(115200);

    // Inicializar el bus I2C y el chip táctil AXS5106 en los pines reales 42 y 41
    touch_init();

    Preferences prefs;
    prefs.begin("tama-kernel", false); 
    debugMode = prefs.getBool("debug", false);
    prefs.end();

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    if (!gfx->begin()) {
        Serial.println("Error inicializando pantalla física.");
    }
    
    bus->beginWrite();
    bus->writeCommand(0x36);
    bus->write(0x48); // Orientación de hardware vertical
    bus->endWrite();

    gfx->fillScreen(0x0000);

    // Inicializar lector de tarjetas SD mediante SD_MMC
    SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("Advertencia: No se pudo montar la tarjeta SD mediante SD_MMC.");
    } else {
        Serial.println("Tarjeta SD (SD_MMC) montada correctamente.");
        AssetManager::getInstance().setFileSystem(&SD_MMC);

        // PERSISTENCIA NIX: Comprobar presencia física de DEBUG.txt en la tarjeta SD
        if (SD_MMC.exists("/DEBUG.txt")) {
            debugMode = true;
            Serial.println("[KERNEL] Archivo /DEBUG.txt detectado en la SD. MODO DEBUG ACTIVADO.");
        } else {
            debugMode = false;
        }
    }

    AssetManager::getInstance().setPNG(&png);

    // 1. ASIGNACIÓN DINÁMICA E INICIALIZACIÓN DEL JUEGO
    game = new Game(172, 320);
    game->init();

    // --- OPTIMIZACIÓN DE UX: ENVIAR EL PRIMER FRAME DE FORMA INMEDIATA ---
    // Esto dibuja el fondo y la mascota en pantalla en menos de 200 ms de boot,
    // garantizando que el usuario vea la mascota y eliminando cualquier retraso visual.
    game->tick();
    game->flush(gfx);

    // 2. SOLO AHORA QUE LA PANTALLA YA SE MUESTRA PERFECTA, INICIAMOS LA RED
    Serial.println("[KERNEL] Cargando multiples redes WiFi guardadas...");
    WiFi.mode(WIFI_STA);
    
    bool tieneRedes = cargarRedesSD();

    if (tieneRedes) {
        // Modo Conectando: Iniciar negociación no bloqueante en segundo plano mientras la mascota se anima
        wifiMulti.run();
        connectionStartTime = millis();
        currentState = STATE_CONNECTING;
    } else {
        // Modo Sin Redes: Desactivar WiFi instantáneamente y tirar el Popup de Stardew sobre el fondo que ya dibujamos
        Serial.println("[KERNEL] No se detectaron redes validas. Saltando al Popup de inmediato.");
        WiFi.mode(WIFI_OFF);
        currentState = STATE_POPUP;
        dibujarPopupStardew(); // El popup se dibuja encima del juego renderizado
        popupLaunchTime = millis(); // Iniciar cronómetro contra toques de calibración
    }
}

void loop() {
    // --- TIME SLICING: SOLO RENDERIZAR SI ESTAMOS EN GAMEPLAY O CONECTANDO ---
    if (game && (currentState == STATE_GAMEPLAY || currentState == STATE_CONNECTING)) {
        game->tick();
        game->flush(gfx);
    }

    // --- MÁQUINA DE ESTADOS DEL KERNEL ---
    switch (currentState) {
        case STATE_CONNECTING: {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[KERNEL] WiFi conectado con exito.");
                currentState = STATE_GAMEPLAY;
            } else {
                wifiMulti.run();
            }

            // Timeout de 15 segundos para dar margen al escaneo de múltiples redes
            if (millis() - connectionStartTime > 15000) {
                Serial.println("[KERNEL] Timeout de WiFi excedido. Lanzando Popup interactivo.");
                WiFi.mode(WIFI_OFF); // Apagar WiFi de inmediato para liberar RAM
                currentState = STATE_POPUP;
                dibujarPopupStardew(); // Dibujar el Popup estilo Stardew Valley
                popupLaunchTime = millis();
            }
            break;
        }

        case STATE_GAMEPLAY: {
            break;
        }

        case STATE_POPUP: {
            // El juego está pausado de forma completa y segura en el bus SPI
            int seleccion = leerClickPopup();
            if (seleccion == 1) { // "Configurar" (Online)
                Serial.println("[KERNEL] Cargando Portal Cautivo...");
                currentState = STATE_PORTAL;
                
                // --- DESALOJAR JUEGO Y RENDERER COMPLETO DE LA RAM DE INMEDIATO ---
                if (game) {
                    delete game; // Libera framebuffer y backing store de inmediato (151 KB de RAM recuperados)
                    game = nullptr;
                }
                SceneManager::getInstance().changeScene(nullptr);
                AssetManager::getInstance().clearUnused();

                // Dibujar la interfaz del portal cautivo
                dibujarPantallaPortal();
                
                // Configuración con credenciales requeridas por el usuario
                TamaNetworkManager::Config netCfg = {
                    .gfx = gfx,
                    .png = &png,
                    .qrPath = "/QR Network.png",
                    .apSSID = "TamaConfig",
                    .apPassword = "iluvUiluvU<3",
                    .pngOpen = pngOpen,
                    .pngClose = pngClose,
                    .pngRead = pngRead,
                    .pngSeek = pngSeek,
                    .pngDraw = qrDrawCallback,
                    .checkExit = checkExitCallback
                };
                TamaNetworkManager netManager(netCfg);
                netManager.begin();
                bool completado = netManager.runCaptivePortal();

                if (!completado) {
                    // --- FILOSOFÍA NIX: RESTAURACIÓN MEDIANTE REINICIO ULTRA-RÁPIDO ---
                    Serial.println("[KERNEL] Configuración cancelada. Reiniciando de forma segura...");
                    delay(500);
                    ESP.restart();
                } else {
                    ESP.restart();
                }
            }
            else if (seleccion == 2) { // "Jugar Offline"
                Serial.println("[KERNEL] Seleccionado modo Offline. Continuando...");
                currentState = STATE_GAMEPLAY;
            }
            break;
        }

        case STATE_PORTAL: {
            break;
        }
    }

    // Escucha de comandos Serial (Case-Insensitive)
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
        else if (cmd == "WIFI") {
            Serial.println("\n[KERNEL] Iniciando Portal Cautivo Manual. Desalojando RAM...");
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
                .apSSID = "TamaConfig",
                .apPassword = "iluvUiluvU<3",
                .pngOpen = pngOpen,
                .pngClose = pngClose,
                .pngRead = pngRead,
                .pngSeek = pngSeek,
                .pngDraw = qrDrawCallback,
                .checkExit = checkExitCallback
            };
            TamaNetworkManager netManager(netCfg);
            netManager.begin();
            bool completado = netManager.runCaptivePortal();

            if (!completado) {
                currentState = STATE_GAMEPLAY;
                game = new Game(172, 320);
                game->init();
            } else {
                ESP.restart();
            }
        }
    }

    delay(20);
}
