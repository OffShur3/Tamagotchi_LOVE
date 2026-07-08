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
#include "PhotoCarousel.h"

int pngOffsetX = 0;
int pngOffsetY = 0;

#define TFT_BL 46

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
  
  int drawWidth = pDraw->iWidth;
  
  if (pngOffsetX + drawWidth > gfx->width()) {
    drawWidth = gfx->width() - pngOffsetX;
  }
  
  png.getLineAsRGB565(pDraw, globalLineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  gfx->draw16bitRGBBitmap(pngOffsetX, y, globalLineBuffer, drawWidth, 1);
  return 1;
}

// game
Game* game = nullptr;

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

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inicialización de hardware base
  pinMode(TFT_BL, OUTPUT);
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
  game = new PhotoCarousel(gfx, &png, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
  game->init();
}

void loop() {
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

  // borrar redes si se recibe el comando "BORRAR" por el puerto serie
  if (Serial.available() > 0) {
    String comando = Serial.readStringUntil('\n');
    comando.trim(); 

    if (comando.equalsIgnoreCase("BORRAR")) {
      borrarRedes();
      delay(500);
      ESP.restart(); 
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
