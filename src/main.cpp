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

volatile bool peticionDormir = false; // "volatile" es vital para interrupciones

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
    // Definimos un color que actuará como transparente (ej. un fucsia o un negro puro 0x0000)
    // Usaremos 0x0000 y configuraremos getLineAsRGB565 para que lo use en zonas transparentes.
    uint16_t transparentKey = 0x0000; 

    // 1. Obtener la línea. El último parámetro es el color que se usará para los píxeles transparentes.
    png.getLineAsRGB565(pDraw, globalLineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0ULL); 
    // Nota: El valor 0ULL aquí le dice a la librería que si hay transparencia, rellene con 0 (negro).

    if (isSpriteSheet) {
        // --- MODO BICHO (Escalado x2 + Crop + Transparencia por Chroma Key) ---
        int sourceXStart = currentFrame * spriteWidth;
        int screenY = pngOffsetY + (pDraw->y * 2);

        if (screenY >= gfx->height()) return 0;

        for (int x = 0; x < spriteWidth; x++) {
            uint16_t pixel = globalLineBuffer[sourceXStart + x];
            
            // En lugar de usar pAlpha, comparamos contra nuestra clave de transparencia
            // Si el PNG tiene canal alfa, getLineAsRGB565 lo procesará y pondrá 0 donde sea transparente
            if (pixel != transparentKey) {
                gfx->writePixel(pngOffsetX + (x * 2),     screenY,     pixel);
                gfx->writePixel(pngOffsetX + (x * 2) + 1, screenY,     pixel);
                gfx->writePixel(pngOffsetX + (x * 2),     screenY + 1, pixel);
                gfx->writePixel(pngOffsetX + (x * 2) + 1, screenY + 1, pixel);
            }
        }
    } else {
        // --- MODO NORMAL (Fondo, UI, QR) ---
        int screenY = pngOffsetY + pDraw->y;
        if (screenY >= gfx->height()) return 0;

        // Si es el fondo (isSpriteSheet suele ser false), dibujamos todo
        // Si son iconos con transparencia, usamos el filtro de pixel
        if (pDraw->iHasAlpha) { // Usamos iHasAlpha que el compilador sugirió
            for (int x = 0; x < pDraw->iWidth; x++) {
                uint16_t pixel = globalLineBuffer[x];
                if (pixel != transparentKey) {
                    gfx->drawPixel(pngOffsetX + x, screenY, pixel);
                }
            }
        } else {
            // Imagen opaca (Fondo)
            gfx->draw16bitRGBBitmap(pngOffsetX, screenY, globalLineBuffer, pDraw->iWidth, 1);
        }
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

// Función que se ejecuta en el instante exacto que presionas el botón
void IRAM_ATTR isrBotonBoot() {
    static unsigned long ultimaInterrupcion = 0;
    unsigned long tiempoAhora = millis();
    // Debounce simple: evitar rebotes mecánicos del botón
    if (tiempoAhora - ultimaInterrupcion > 200) { 
        peticionDormir = true;
    }
    ultimaInterrupcion = tiempoAhora;
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
  // Adjuntar la interrupción al pin 0 (FALLING = cuando se presiona)
  attachInterrupt(digitalPinToInterrupt(BOOT_PIN), isrBotonBoot, FALLING);
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
  // 1. Revisar el flag de la interrupción al inicio del loop
  if (peticionDormir) {
      peticionDormir = false; // Resetear flag
      irADormir();
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

  static bool networkPopupShown = false;
  if (!redConfigurada && !networkPopupShown && !updateInProgress && !mandatoryUpdate) {
      // Evitar que se muestre justo al arrancar (dar tiempo a iniciar)
      static unsigned long bootTime = millis();
      if (millis() - bootTime > 10000) { // Esperar 3 segundos tras el boot
          bool conecto = pantallaOpcionRed();
          networkPopupShown = true;
          // Forzar redibujado del juego al volver del popup/portal
          if (game) game->redraw();
      }
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
