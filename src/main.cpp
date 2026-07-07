#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "touch_axs5106.h"
#include <FS.h>
#include "colors.h"
#include <SD_MMC.h>
#include <PNGdec.h>

// ============ PINOUT ============
#define TFT_BL 46

#define BGR_RED    0x001F
#define BGR_GREEN  0x07E0
#define BGR_BLUE   0xF800
#define BGR_WHITE  0xFFFF
#define BGR_BLACK  0x0000
#define BGR_YELLOW 0x07FF

#define SD_CLK 16
#define SD_CMD 15
#define SD_D0  17

// ============ PANTALLA ============
Arduino_DataBus *bus = new Arduino_ESP32SPI(45 /* DC */, 21 /* CS */, 38 /* SCK */, 39 /* MOSI */);
Arduino_GFX *gfx = new Arduino_ST7789(
    bus, 40 /* RST */, 0 /* rotation */, false /* IPS */,
    172 /* width */, 320 /* height */,
    34 /* col_offset1 */, 0 /* row_offset1 */,
    34 /* col_offset2 */, 0 /* row_offset2 */);

// ============ PNG DECODER ============
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
  if (pDraw->y >= gfx->height()) return 0;
  int drawWidth = pDraw->iWidth;
  if (drawWidth > gfx->width()) drawWidth = gfx->width();
  png.getLineAsRGB565(pDraw, globalLineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  gfx->draw16bitRGBBitmap(0, pDraw->y, globalLineBuffer, drawWidth, 1);
  return 1;
}

// ============ TOUCH (corregido) ============
bool leer_touch_corregido(uint16_t &x, uint16_t &y) {
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

// ============ GESTIÓN DE IMÁGENES ============
const int MAX_IMAGES = 20;
String imageFiles[MAX_IMAGES];
int imageCount = 0;
int currentImage = 0;
bool fallbackMode = false;   // true si no hay SD o no hay imágenes

// Dibuja la imagen `index` del array
void drawImage(int index) {
  if (fallbackMode || imageCount == 0 || index < 0 || index >= imageCount) return;

  gfx->fillScreen(BGR_BLACK);
  String fullPath = "/" + imageFiles[index];
  int rc = png.open(fullPath.c_str(), pngOpen, pngClose, pngRead, pngSeek, pngDraw);
  if (rc == PNG_SUCCESS) {
    png.decode(NULL, 0);
    png.close();
    Serial.printf("Mostrando imagen %d: %s\n", index, imageFiles[index].c_str());
  } else {
    Serial.printf("Error al abrir %s (código %d)\n", imageFiles[index].c_str(), rc);
  }
}

// Dibuja el fallback: líneas de colores + coordenadas
void drawFallback() {
  gfx->fillScreen(BGR_BLACK);
  int h = gfx->height();
  int step = h / 6;
  uint16_t colors[] = {BGR_RED, BGR_GREEN, BGR_BLUE, BGR_YELLOW, BGR_WHITE, BGR_BLACK};
  for (int i = 0; i < 6; i++) {
    gfx->fillRect(0, i * step, gfx->width(), step, colors[i % 6]);
  }
  // Preparamos un área central para las coordenadas
  gfx->setTextSize(2);
  gfx->setTextColor(BGR_WHITE, BGR_BLACK);
  // Se actualizará en el loop con las coordenadas
}

// Actualiza el texto de coordenadas en el fallback
void updateFallbackCoords(uint16_t x, uint16_t y) {
  // Borra el área donde escribimos (rectángulo negro)
  int boxW = 160;
  int boxH = 30;
  int boxX = (gfx->width() - boxW) / 2;
  int boxY = (gfx->height() - boxH) / 2;
  gfx->fillRect(boxX, boxY, boxW, boxH, BGR_BLACK);
  gfx->setCursor(boxX + 10, boxY + 8);
  gfx->setTextColor(BGR_WHITE);
  gfx->printf("X:%03d Y:%03d", x, y);
}

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== CARRUSEL DE IMÁGENES CON DESLIZAMIENTO ===");

  // Encender backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  gfx->begin();

  // Forzar modo BGR
  bus->beginWrite();
  bus->writeCommand(0x36);
  bus->write(0x48); // 0x48 = BGR
  bus->endWrite();

  // Inicializar touch
  touch_init();

  // Inicializar SD
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
  if (SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD OK. Escaneando archivos PNG...");
    File root = SD_MMC.open("/");
    File file = root.openNextFile();
    while (file && imageCount < MAX_IMAGES) {
      if (!file.isDirectory()) {
        String name = file.name();
        name.toLowerCase();
        if (name.endsWith(".png")) {
          imageFiles[imageCount++] = file.name();
          Serial.printf("  - %s\n", file.name());
        }
      }
      file = root.openNextFile();
    }
    root.close();
    Serial.printf("Total de imágenes PNG: %d\n", imageCount);
  } else {
    Serial.println("Fallo al inicializar SD.");
  }

  // Decidir modo
  if (imageCount == 0) {
    fallbackMode = true;
    drawFallback();
    Serial.println("Modo fallback: sin imágenes.");
  } else {
    fallbackMode = false;
    currentImage = 0;
    drawImage(0);
  }
}

// ============ LOOP ============
void loop() {
  static uint16_t lastX = 0, lastY = 0;
  static uint16_t touchStartX = 0, touchStartY = 0;
  static bool touching = false;
  static unsigned long lastGestureTime = 0;
  const unsigned long GESTURE_COOLDOWN = 200; // ms

  uint16_t x = 0, y = 0;
  bool touched = leer_touch_corregido(x, y);

  if (touched) {
    lastX = x;
    lastY = y;

    if (!touching) {
      // Inicio del toque
      touching = true;
      touchStartX = x;
      touchStartY = y;
    }
    // Si estamos en fallback, actualizamos coordenadas en pantalla
    if (fallbackMode) {
      updateFallbackCoords(x, y);
    }
  } else {
    if (touching) {
      // Se soltó el dedo → evaluar deslizamiento
      touching = false;
      unsigned long now = millis();
      if (now - lastGestureTime > GESTURE_COOLDOWN && !fallbackMode) {
        int deltaX = (int)lastX - (int)touchStartX;
        if (abs(deltaX) > 30) {   // umbral de deslizamiento
          if (deltaX > 0) {
            // Deslizamiento hacia la derecha → siguiente imagen
            currentImage = (currentImage + 1) % imageCount;
          } else {
            // Deslizamiento hacia la izquierda → anterior
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
