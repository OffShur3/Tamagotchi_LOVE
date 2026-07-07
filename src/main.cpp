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

const int MAX_IMAGES = 20;
String imageFiles[MAX_IMAGES];
int imageCount = 0;
int currentImage = 0;
bool fallbackMode = false;

void drawImage(int index) {
  if (fallbackMode || imageCount == 0 || index < 0 || index >= imageCount) return;

  gfx->fillScreen(MAT_BG);
  String fullPath = "/" + imageFiles[index];
  int rc = png.open(fullPath.c_str(), pngOpen, pngClose, pngRead, pngSeek, pngDraw);
  if (rc == PNG_SUCCESS) {
    png.decode(NULL, 0);
    png.close();
  }
}

void drawFallback() {
  gfx->fillScreen(MAT_BG);
  int h = gfx->height();
  int step = h / 6;
  uint16_t colors[] = {BGR_RED, BGR_GREEN, BGR_BLUE, BGR_YELLOW, BGR_WHITE, BGR_BLACK};
  for (int i = 0; i < 6; i++) {
    gfx->fillRect(0, i * step, gfx->width(), step, colors[i % 6]);
  }
  gfx->setTextSize(2);
  gfx->setTextColor(BGR_WHITE, BGR_BLACK);
}

void updateFallbackCoords(uint16_t x, uint16_t y) {
  int boxW = 160;
  int boxH = 30;
  int boxX = (gfx->width() - boxW) / 2;
  int boxY = (gfx->height() - boxH) / 2;
  gfx->fillRect(boxX, boxY, boxW, boxH, BGR_BLACK);
  gfx->setCursor(boxX + 10, boxY + 8);
  gfx->setTextColor(BGR_WHITE);
  gfx->printf("Conecta la SD para comenzar", x, y);
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

  gfx->fillScreen(MAT_BG);
  gfx->setTextColor(BGR_WHITE); 
  gfx->setTextSize(2);
  imprimirCentrado("This is 4", 130, 2);
  imprimirCentrado("u babe...", 160, 2);
  delay(1500);
  gfx->fillScreen(MAT_BG);


  // 1. Espera activa por SD
  if (esperarSD()) {
    // Escanear archivos solo si la SD respondió
    File root = SD_MMC.open("/");
    File file = root.openNextFile();
    while (file && imageCount < MAX_IMAGES) {
      if (!file.isDirectory()) {
        String name = file.name();
        name.toLowerCase();
        if (name.endsWith(".png") && name != "qr network.png") { 
          imageFiles[imageCount++] = file.name();
        }
      }
      file = root.openNextFile();
    }
    root.close();
  }

  // 2. Gestión de red (Solo después de haber resuelto lo de la SD)
  inicializarRed(); 

  // 3. Inicio del juego
  if (imageCount == 0) {
    fallbackMode = true;
    drawFallback();
  } else {
    fallbackMode = false;
    currentImage = 0;
    drawImage(0);
  }
}

void loop() {

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

  // Manejo de gestos de deslizamiento
  static uint16_t lastX = 0, lastY = 0;
  static uint16_t touchStartX = 0, touchStartY = 0;
  static bool touching = false;
  static unsigned long lastGestureTime = 0;
  const unsigned long GESTURE_COOLDOWN = 200; 

  uint16_t x = 0, y = 0;
  bool touched = leerTouch(x, y);

  if (touched) {
    lastX = x;
    lastY = y;

    if (!touching) {
      touching = true;
      touchStartX = x;
      touchStartY = y;
    }
    if (fallbackMode) {
      updateFallbackCoords(x, y);
    }
  } else {
    if (touching) {
      touching = false;
      unsigned long now = millis();
      if (now - lastGestureTime > GESTURE_COOLDOWN && !fallbackMode) {
        int deltaX = (int)lastX - (int)touchStartX;
        if (abs(deltaX) > 30) {   
          if (deltaX > 0) {
            currentImage = (currentImage + 1) % imageCount;
          } else {
            currentImage = (currentImage - 1 + imageCount) % imageCount;
          }
          drawImage(currentImage);
          lastGestureTime = now;
        }
      }
    }
  }

  delay(20);
}
