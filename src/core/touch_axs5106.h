#ifndef TOUCH_AXS5106_H
#define TOUCH_AXS5106_H

#include <Arduino.h>
#include <Wire.h>

#define AXS5106_ADDR 0x63

static bool touch_init() {
    pinMode(47, OUTPUT);
    digitalWrite(47, HIGH); delay(5);
    digitalWrite(47, LOW);  delay(20);
    digitalWrite(47, HIGH); delay(100);

    Wire.begin(42, 41);
    Wire.setClock(100000); 
    return true;
}

static bool touch_is_pressed() {
    Wire.beginTransmission(AXS5106_ADDR);
    Wire.write(0x01); // Leer registro de cantidad de dedos
    if (Wire.endTransmission() != 0) return false;
    
    Wire.requestFrom((uint8_t)AXS5106_ADDR, (uint8_t)1);
    if (Wire.available()) {
        uint8_t fingers = Wire.read();
        return (fingers > 0 && fingers < 5); // Retorna true si detecta un dedo
    }
    return false;
}

static void touch_get_xy(uint16_t *x, uint16_t *y) {
    uint8_t buf[4] = {0};
    Wire.beginTransmission(AXS5106_ADDR);
    Wire.write(0x02); // Iniciar la lectura de coordenadas desde X High (0x02)
    Wire.endTransmission();
    
    Wire.requestFrom((uint8_t)AXS5106_ADDR, (uint8_t)4);
    for (int i = 0; i < 4; i++) {
        if (Wire.available()) buf[i] = Wire.read();
    }
    
    *x = ((buf[0] & 0x0F) << 8) | buf[1];
    *y = ((buf[2] & 0x0F) << 8) | buf[3];
}

#endif
