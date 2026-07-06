#include <TFT_eSPI.h>
TFT_eSPI tft;

void setup() {
  // Backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Reset manual (por si acaso)
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW);
  delay(10);
  digitalWrite(TFT_RST, HIGH);
  delay(120);

  tft.init();
  tft.invertDisplay(false);   // Forzamos que NO esté invertida (por si la macro falla)
  tft.setRotation(0);

  // Ahora estos colores deberían verse correctos:
  tft.fillScreen(TFT_BLACK);      // Debe verse NEGRO
  tft.drawLine(0, 0, 171, 319, TFT_WHITE);  // Línea BLANCA
  tft.drawCircle(86, 160, 50, TFT_RED);     // Círculo ROJO
}

void loop() {}
