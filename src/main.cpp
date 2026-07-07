#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "touch_axs5106.h"

#define TFT_BL 46

#define BGR_RED    0x001F
#define BGR_GREEN  0x07E0
#define BGR_BLUE   0xF800
#define BGR_WHITE  0xFFFF
#define BGR_BLACK  0x0000
#define BGR_YELLOW 0x07FF

Arduino_DataBus *bus = new Arduino_ESP32SPI(45 /* DC */, 21 /* CS */, 38 /* SCK */, 39 /* MOSI */);
Arduino_GFX *gfx = new Arduino_ST7789(
    bus, 40 /* RST */, 0 /* rotation */, false /* IPS */,
    172 /* width */, 320 /* height */,
    34 /* col_offset1 */, 0 /* row_offset1 */,
    34 /* col_offset2 */, 0 /* row_offset2 */);

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
      
      // INVERSIÓN DEL EJE X DEL TÁCTIL
      // Evitamos que números negativos rompan la variable si el raw_x se pasa un poco del borde
      x = (raw_x > 172) ? 0 : (172 - raw_x);
      y = raw_y;
      
      return true;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== Test Paso 2: Pantalla + Touch DES-ESPEJADO ===");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  gfx->begin();

  // --- FIX ESPEJADO DE PANTALLA ---
  // Forzamos el registro MADCTL (0x36) para voltear el eje X a nivel hardware
  bus->beginWrite();
  bus->writeCommand(0x36);
  bus->write(0x40); // 0x40 es la bandera MX (Mirror X)
  bus->endWrite();
  // --------------------------------

  int barHeight = gfx->height() / 6;
  gfx->fillRect(0, barHeight * 0, gfx->width(), barHeight, BGR_RED);
  gfx->fillRect(0, barHeight * 1, gfx->width(), barHeight, BGR_GREEN);
  gfx->fillRect(0, barHeight * 2, gfx->width(), barHeight, BGR_BLUE);
  gfx->fillRect(0, barHeight * 3, gfx->width(), barHeight, BGR_WHITE);
  gfx->fillRect(0, barHeight * 4, gfx->width(), barHeight, BGR_BLACK);
  gfx->fillRect(0, barHeight * 5, gfx->width(), barHeight, BGR_YELLOW);

  touch_init();
  gfx->setTextSize(2);
}

void loop() {
  uint16_t x = 0, y = 0;
  
  if (leer_touch_corregido(x, y)) {
    Serial.printf("Tocado en X:%d Y:%d\n", x, y);

    int boxW = 140;
    int boxH = 40;
    int boxX = (gfx->width() - boxW) / 2;
    int boxY = (gfx->height() - boxH) / 2;
    
    gfx->fillRect(boxX, boxY, boxW, boxH, BGR_BLACK);
    
    gfx->setCursor(boxX + 10, boxY + 12);
    gfx->setTextColor(BGR_WHITE);
    gfx->printf("X:%03d Y:%03d", x, y);
  }
  
  delay(20);
}
