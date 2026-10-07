// src/ColorTuner.h
#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <PNGdec.h>

enum ColorDomain : uint8_t {
    DOMAIN_TAMA = 0,    // Sprites de la criatura (/sprites/base/)
    DOMAIN_WORLD,       // Fondo (bg_main.png) y objetos ambientales (caca)
    DOMAIN_BUTTONS,     // Botones inferiores (icons_ui.png)
    DOMAIN_HUD,         // Barras de StatusHUD y reloj ClockWidget
    DOMAIN_THOUGHT,     // Nube ThoughtBubble e iconos 7x7
    DOMAIN_POPUP_DAY,   // Cuadros de diálogo diurnos
    DOMAIN_POPUP_DREAM, // Cuadro de diálogo onírico
    DOMAIN_COUNT
};

struct ColorProfile {
    int8_t deltaR = 0;        // -100 a +100
    int8_t deltaG = 0;        // -100 a +100
    int8_t deltaB = 0;        // -100 a +100
    int8_t brightness = 0;    // -100 a +100 (Luz en espacio HSL)
    int8_t contrast = 0;      // -100 a +100 (Contraste centrado en L=0.5)
    int8_t saturation = 0;    // -100 a +100 (Saturación cromática S)
    bool swapRB = false;      // Invierte canales R y B exclusivamente en este dominio
    bool invert = false;      // Invierte colores exclusivamente en este dominio

    bool isNeutral() const {
        return (deltaR == 0 && deltaG == 0 && deltaB == 0 &&
                brightness == 0 && contrast == 0 && saturation == 0 &&
                !swapRB && !invert);
    }
};

ColorProfile& getGlobalProfile();

// Pipeline de graduación de color perceptivo desacoplado
uint16_t colorTunerApply(ColorDomain domain, uint8_t r, uint8_t g, uint8_t b);
uint16_t colorTunerCorrect565(ColorDomain domain, uint16_t c565);
bool colorTunerHasEffect(ColorDomain domain);

// Calibración individual color por color
void setColorOverride(uint16_t color565, const ColorProfile& prof);
bool hasColorOverride(uint16_t color565);
void removeColorOverride(uint16_t color565);
void clearColorOverrides();

// Acceso directo a perfiles por dominio
ColorProfile& getColorProfile(ColorDomain domain);
const char* getColorDomainName(ColorDomain domain);
ColorDomain parseColorDomain(const String& name);

// Banderas de hardware y luz nocturna
bool getHardwareBGR();
bool getHardwareInv();
bool getHardwareSwapBytes();
uint8_t getNightDimPercent();
void setNightDimPercent(uint8_t val);
uint16_t applyNightDim565(uint16_t c565);

// Persistencia en SD y control del calibrador
void loadColorConfigSD(Arduino_DataBus* bus = nullptr);
void saveColorConfigSD();
void factoryResetColorSD(Arduino_DataBus* bus = nullptr);
String getRawConfigString();
void setRawConfigString(const String& raw);
void runColorTuner(Arduino_GFX* gfx, Arduino_DataBus* bus, PNG* pngInstance);
String serializeProfilesJson();
