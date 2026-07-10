// Tamagotchi-LOVE

// Librerias
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <FS.h>
#include <SD_MMC.h>
#include <PNGdec.h>
#include "NetworkManager.h"

// Archivos
#include "touch_axs5106.h"
#include "colors.h"
#include "UI.h"
#include "UpdateManager.h"
#include "Tamagotchi.h"
// #include "PhotoCarousel.h"


// game
int pngOffsetX = 0;
int pngOffsetY = 0;
// Variables para controlar el recorte (Crop) del Sprite Sheet
int spriteWidth = 48;   // Ancho de un solo frame
int spriteHeight = 48;  // Alto de un solo frame
int currentFrame = 0;   // Frame actual (0, 1, 2, 3...)
// --- Variables Globales nuevas/modificadas en main.cpp ---
bool isSpriteSheet = false; // Flag para saber si aplicar recorte y escalado
uint16_t scaledLineBuffer[1024]; // Buffer más grande para el escalado x2
Game* game = nullptr;
#define BOOT_PIN 0 // Deep Sleep

// TFT
#define TFT_BL 46
// SD
#define SD_CLK 16
#define SD_CMD 15
#define SD_D0  17

Arduino_DataBus *bus = new Arduino_ESP32SPI(45, 21, 38, 39);
Arduino_GFX *gfx = new Arduino_ST7789(
    bus, 40, 0, false,
    172, 320,
    34, 0,
    34, 0);

PNG png;
uint16_t globalLineBuffer[512];

void *pngOpen(const char *filename, int32_t *size) {
  File *f = new File(SD_MMC.open(filename, "r"));
  if (!f || !*f) return nullptr;
  *size = f->size();
  return (void *)f;
}

void pngClose(void *handle) {
  File *f = (File *)handle;
  if (f) { f->close(); delete f; }
}

int32_t pngRead(PNGFILE *handle, uint8_t *buffer, int32_t length) {
  if (!handle) return 0;
  File *f = (File *)handle->fHandle;
  return f->read(buffer, length);
}

int32_t pngSeek(PNGFILE *handle, int32_t position) {
  if (!handle) return -1;
  File *f = (File *)handle->fHandle;
  return f->seek(position) ? position : -1;
}

int pngDraw(PNGDRAW *pDraw) {
    int y = pDraw->y + pngOffsetY;
    if (y >= gfx->height()) return 0;

    png.getLineAsRGB565(pDraw, globalLineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);

    if (isSpriteSheet) {
        // --- ESCALADO x2 Y RECORTE ---
        int sourceX = currentFrame * spriteWidth;
        
        // 1. Escalar horizontalmente al buffer auxiliar
        for (int i = 0; i < spriteWidth; i++) {
            uint16_t pixel = globalLineBuffer[sourceX + i];
            scaledLineBuffer[i * 2] = pixel;
            scaledLineBuffer[i * 2 + 1] = pixel;
        }

        // 2. Dibujar la línea actual
        gfx->draw16bitRGBBitmap(pngOffsetX, y, scaledLineBuffer, spriteWidth * 2, 1);
        
        // 3. Duplicar la línea verticalmente si hay espacio
        if (y + 1 < gfx->height()) {
            gfx->draw16bitRGBBitmap(pngOffsetX, y + 1, scaledLineBuffer, spriteWidth * 2, 1);
        }
        
        // Ajustamos el offset interno para la siguiente línea de PNGdec
        // Esto compensa que estamos dibujando de a 2 pixeles de alto
        pngOffsetY++; 

    } else {
        // --- DIBUJO NORMAL (Fondo e Iconos) ---
        gfx->draw16bitRGBBitmap(pngOffsetX, y, globalLineBuffer, pDraw->iWidth, 1);
    }
    return 1;
}

bool leerTouch(uint16_t &x, uint16_t &y) {
  Wire.beginTransmission(0x63);
  Wire.write(0x02);
  Wire.endTransmission();
  uint8_t bytesLeidos = Wire.requestFrom((uint8_t)0x63, (uint8_t)5);
  if (bytesLeidos >= 5) {
    uint8_t dedos = Wire.read();
    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();
    if (dedos > 0 && dedos < 3) {
      uint16_t raw_x = ((xh & 0x0F) << 8) | xl;
      uint16_t raw_y = ((yh & 0x0F) << 8) | yl;
      x = (raw_x > 172) ? 0 : (172 - raw_x);
      y = raw_y;
      return true;
    }
  }
  return false;
}

// Función para verificar la SD en un bucle
bool esperarSD() {
  // Pantalla de espera
  gfx->fillScreen(MAT_BG);
  imprimirCentrado("SD", 130, 2);
  imprimirCentrado("no detectada", 160, 2);
  imprimirCentrado("Por favor insertela", 190, 1);
     
  while (true) {
    SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
    if (SD_MMC.begin("/sdcard", true)) {
      // SD detectada, salir del bucle
      return true;
    }

    delay(500);
  }
}

void irADormir() {
    Serial.println("[POWER] Guardando y entrando en Deep Sleep...");
    
    // 1. Guardar partida si el juego existe
    if (game) {
        static_cast<Tamagotchi*>(game)->saveGame();
    }

    // 2. Apagar pantalla y periféricos
    digitalWrite(TFT_BL, LOW);
    gfx->displayOff();

    // 3. Configurar GPIO 0 como fuente de despertar (si se presiona de nuevo)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BOOT_PIN, 0); // 0 = Low
    
    // 4. Dormir
    esp_deep_sleep_start();
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inicialización de hardware base
  pinMode(TFT_BL, OUTPUT);
  pinMode(BOOT_PIN, INPUT_PULLUP); // Pin de botón del Deep Sleep
  digitalWrite(TFT_BL, HIGH);
  gfx->begin();
  bus->beginWrite();
  bus->writeCommand(0x36);
  bus->write(0x48); 
  bus->endWrite();
  touch_init();

  // Pantalla de bienvenida
  gfx->fillScreen(MAT_BG);
  gfx->setTextColor(BGR_WHITE);
  gfx->setTextSize(2);
  imprimirCentrado("This is 4", 130, 2);
  imprimirCentrado("u babe...", 160, 2);
  delay(1500);

  // 1. Esperar a que la SD esté disponible
  esperarSD();

  // 2. Crear el juego e inicializarlo
  game = new Tamagotchi(gfx, &png, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
  game->init();
}

void loop() {
  // 1. Verificar Botón BOOT para dormir
  if (digitalRead(BOOT_PIN) == LOW) {
      unsigned long pressTime = millis();
      while(digitalRead(BOOT_PIN) == LOW) { delay(10); } // Debounce
      if (millis() - pressTime > 100) { // Si fue un click real
          irADormir();
      }
  }

  // Leer touch al inicio (necesario para badge y juego)
  uint16_t x = 0, y = 0;
  bool touched = leerTouch(x, y);


  // 1. Intentar conectar WiFi en segundo plano (sin bloquear)
  static unsigned long lastWiFiAttempt = 0;
  if (!redConfigurada && millis() - lastWiFiAttempt > 10000) { // cada 10 segundos
    lastWiFiAttempt = millis();
    inicializarRed();
  }

  // Actualización obligatoria al inicio (solo una vez)
  static bool mandatoryUpdateDone = false;
  if (!mandatoryUpdateDone && WiFi.status() == WL_CONNECTED) {
    mandatoryUpdateDone = true;
    // Obtener la última versión disponible en GitHub
    checkForUpdate();
    // Si hay una actualización obligatoria, activar el badge
    if (needMandatoryUpdate()) {
      Serial.println("[MAIN] Actualización obligatoria requerida");
      updateAvailable = true;
      mandatoryUpdate = true;
      // No ejecutar performFullUpdate directamente
    } else {
      Serial.println("[MAIN] No se requiere actualización obligatoria");
    }
  }

  // Verificar actualizaciones periódicamente (solo si no hay actualización en progreso)
  static unsigned long lastUpdateCheck = 0;
  if (WiFi.status() == WL_CONNECTED && !updateInProgress && !updateAvailable &&
      millis() - lastUpdateCheck > UPDATE_CHECK_INTERVAL) {
    Serial.println("[MAIN] Verificando actualizaciones...");
    lastUpdateCheck = millis();
    checkForUpdate();
  }

  // Manejo de Comandos Serial
  if (Serial.available() > 0) {
    String comando = Serial.readStringUntil('\n');
    comando.trim(); 

    if (comando.equalsIgnoreCase("BORRAR")) {
      borrarRedes();
      delay(500);
      ESP.restart(); 
    }
    
    if (comando.equalsIgnoreCase("RESET")) {
      if (game) {
        static_cast<Tamagotchi*>(game)->resetGame();
      }
    }
  }

  // Dibujar badge de actualización si está disponible
  if (updateAvailable && !updateInProgress) {
    drawUpdateBadge();
    if (touched && isTouchingBadge(x, y)) {
      esperarSoltar();
      showUpdatePopup();
    }
  }

  // JUEGO
  if (game) {
    game->update(x, y, touched);
  }
    
  delay(20);
}
//
