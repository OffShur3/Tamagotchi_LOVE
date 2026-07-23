#ifndef TOUCH_AXS5106_H
#define TOUCH_AXS5106_H

#include <Arduino.h>
#include <Wire.h>

#define AXS5106_ADDR 0x63

static bool touch_init() {
    pinMode(47, OUTPUT);
    digitalWrite(47, LOW);  delay(20);
    digitalWrite(47, HIGH); delay(100);

    Wire.begin(42, 41);
    Wire.setClock(100000); 
    return true;
}

// Lectura atómica de 5 bytes (Evita limpiar registros por separado)
static bool touch_read(uint16_t &x, uint16_t &y) {
    Wire.beginTransmission(AXS5106_ADDR);
    Wire.write(0x02);
    if (Wire.endTransmission() != 0) return false;
    
    uint8_t buf[5] = {0};
    if (Wire.requestFrom((uint8_t)AXS5106_ADDR, (uint8_t)5) == 5) {
        for (int i = 0; i < 5; i++) buf[i] = Wire.read();
        if (buf[0] > 0 && buf[0] < 5) {
            uint16_t raw_x = ((buf[1] & 0x0F) << 8) | buf[2];
            uint16_t raw_y = ((buf[3] & 0x0F) << 8) | buf[4];
            x = (raw_x > 172) ? 0 : (172 - raw_x);
            y = raw_y;
            return true;
        }
    }
    return false;
}

#endif
