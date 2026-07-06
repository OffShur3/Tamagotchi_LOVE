#ifndef TOUCH_AXS5106_H
#define TOUCH_AXS5106_H

#include <Arduino.h>
#include <Wire.h>

#define AXS5106_ADDR       0x5D
#define AXS5106_XY_REG     0x00
#define AXS5106_STATUS_REG 0x02

static bool touch_init() {
    // Pines según esquemático: SDA=42, SCL=41
    Wire.begin(42, 41);  
    
    // Pin de Reset táctil: GPIO 47
    pinMode(47, OUTPUT);
    digitalWrite(47, LOW);
    delay(20);
    digitalWrite(47, HIGH);
    delay(100);
    return true;
}

static bool touch_is_pressed() {
    // ---- PARCHE DE DEPURACIÓN: Ignorar el táctil temporalmente ----
    return false; 
    
    // Todo lo de abajo no se ejecutará por ahora
    Wire.beginTransmission(AXS5106_ADDR);
    Wire.write(AXS5106_STATUS_REG);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom(AXS5106_ADDR, 1);
    if (Wire.available()) {
        uint8_t status = Wire.read();
        return (status & 0x01);
    }
    return false;
}

static void touch_get_xy(uint16_t *x, uint16_t *y) {
    uint8_t buf[4] = {0};
    Wire.beginTransmission(AXS5106_ADDR);
    Wire.write(AXS5106_XY_REG);
    Wire.endTransmission(false);
    Wire.requestFrom(AXS5106_ADDR, 4);
    for (int i = 0; i < 4; i++) {
        if (Wire.available()) buf[i] = Wire.read();
    }
    *x = ((buf[0] & 0x0F) << 8) | buf[1];
    *y = ((buf[2] & 0x0F) << 8) | buf[3];
    // Descomenta y ajusta si necesitas rotar las coordenadas:
    // uint16_t raw_x = *x, raw_y = *y;
    // *x = raw_y;
    // *y = 320 - raw_x;
}

#endif
