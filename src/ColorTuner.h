// src/ColorTuner.h
#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

enum ColorDomain : uint8_t {
    DOMAIN_TAMA = 0,
    DOMAIN_WORLD,
    DOMAIN_BUTTONS,
    DOMAIN_HUD,
    DOMAIN_THOUGHT,
    DOMAIN_POPUP_DAY,
    DOMAIN_POPUP_DREAM,
    DOMAIN_COUNT
};

struct ColorProfile {
    int8_t deltaR;
    int8_t deltaG;
    int8_t deltaB;
    int8_t brightness;
    int8_t contrast;
    int8_t saturation;
    bool swapRB;
    bool invert;

    // Constructor compatible para compiladores C++11
    ColorProfile(int8_t dr = 0, int8_t dg = 0, int8_t db = 0, int8_t bri = 0, int8_t con = 0, int8_t sat = 0, bool sw = false, bool inv = false)
        : deltaR(dr), deltaG(dg), deltaB(db), brightness(bri), contrast(con), saturation(sat), swapRB(sw), invert(inv) {}

    bool isNeutral() const {
        return (deltaR == 0 && deltaG == 0 && deltaB == 0 &&
                brightness == 0 && contrast == 0 && saturation == 0 &&
                !swapRB && !invert);
    }
};

ColorProfile& getGlobalProfile();
uint16_t colorTunerApply(ColorDomain domain, uint8_t r, uint8_t g, uint8_t b);
uint16_t colorTunerCorrect565(ColorDomain domain, uint16_t c565);
bool colorTunerHasEffect(ColorDomain domain);

void setColorOverride(uint16_t color565, const ColorProfile& prof);
bool hasColorOverride(uint16_t color565);
void removeColorOverride(uint16_t color565);
void clearColorOverrides();

ColorProfile& getColorProfile(ColorDomain domain);
const char* getColorDomainName(ColorDomain domain);
ColorDomain parseColorDomain(const String& name);

bool getHardwareBGR();
bool getHardwareInv();
bool getHardwareSwapBytes();
uint8_t getNightDimPercent();
void setNightDimPercent(uint8_t val);
uint16_t applyNightDim565(uint16_t c565);

void loadColorConfigSD(Arduino_DataBus* bus = nullptr);
void saveColorConfigSD();
void factoryResetColorSD(Arduino_DataBus* bus = nullptr);
String getRawConfigString();
void setRawConfigString(const String& raw);
String serializeProfilesJson();
