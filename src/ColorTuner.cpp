// src/ColorTuner.cpp
#include "ColorTuner.h"
#include "render/TextUtil.h"
#include "render/RenderContext.h"
#include "render/Sprite.h"
#include "assets/AssetManager.h"
#include <SD_MMC.h>
#include <unordered_map>
#include <math.h>

namespace {
    static Arduino_DataBus* _bus = nullptr;

    const char* CONFIG_PATH = "/config/color_tuning.txt";

    // --- TUS VALORES HARDCODEADOS IDEALES ---
    static bool currentBGR = true;
    static bool currentInv = false;
    static bool swapPngBytes = false;

    static uint8_t nightDimPercent = 75;
    static bool previewLightsOn = true;

    // Perfil Base Global (bri: 40, con: 30)
    static ColorProfile globalProfile(0, 0, 0, 40, 30, 0, false, false); 
    
    // Perfiles por Dominio predeterminados
    static ColorProfile domainProfiles[DOMAIN_COUNT] = {
        ColorProfile(0, 0, 0, 0, 0, 0, false, false),        // DOMAIN_TAMA
        ColorProfile(0, 0, 0, 0, 0, 0, false, false),        // DOMAIN_WORLD
        ColorProfile(0, 0, 0, -15, -11, 0, false, false),    // DOMAIN_BUTTONS
        ColorProfile(0, 0, 0, -40, -30, 0, false, false),    // DOMAIN_HUD
        ColorProfile(0, 0, 0, 0, 0, 0, false, false),        // DOMAIN_THOUGHT
        ColorProfile(0, 0, 0, -40, -30, 0, false, false),    // DOMAIN_POPUP_DAY
        ColorProfile(0, 0, 0, 0, 0, 0, false, false),        // DOMAIN_POPUP_DREAM
        ColorProfile(0, 0, 0, -40, -30, 0, false, false)     // DOMAIN_PLAZA (Neutraliza el quemado pastel)
    };

    static std::unordered_map<uint16_t, ColorProfile> colorOverrides;
    static ColorDomain activeTunerDomain = DOMAIN_PLAZA;
    static bool tunerActive = false;
    static bool tunerDirty = true;
    static uint8_t curBaseR = 220, curBaseG = 20, curBaseB = 20;

    static const char* DOMAIN_NAMES[DOMAIN_COUNT] = {
        "TAMA", "WORLD", "BUTTONS", "HUD", "THOUGHT", "POPUP_DAY", "POPUP_DREAM", "PLAZA"
    };

    inline uint8_t clampU8(float val) {
        if (val <= 0.0f) return 0;
        if (val >= 255.0f) return 255;
        return (uint8_t)(val + 0.5f);
    }

    inline uint16_t rgbTo565(uint8_t r, uint8_t g, uint8_t b) {
        return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
    }

    static void rgbToHsl(uint8_t r, uint8_t g, uint8_t b, float& h, float& s, float& l) {
        float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
        float maxVal = max(rf, max(gf, bf)), minVal = min(rf, min(gf, bf));
        float delta = maxVal - minVal;

        l = (maxVal + minVal) * 0.5f;

        if (delta <= 0.00001f) {
            h = 0.0f; s = 0.0f; return;
        }

        s = (l > 0.5f) ? (delta / (2.0f - maxVal - minVal)) : (delta / (maxVal + minVal));

        if (maxVal == rf) {
            h = 60.0f * (fmodf(((gf - bf) / delta), 6.0f));
        } else if (maxVal == gf) {
            h = 60.0f * (((bf - rf) / delta) + 2.0f);
        } else {
            h = 60.0f * (((rf - gf) / delta) + 4.0f);
        }
        if (h < 0.0f) h += 360.0f;
    }

    static float hue2rgb(float p, float q, float t) {
        if (t < 0.0f) t += 1.0f;
        if (t > 1.0f) t -= 1.0f;
        if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
        if (t < 1.0f / 2.0f) return q;
        if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
        return p;
    }

    static void hslToRgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (s <= 0.00001f) {
            r = g = b = clampU8(l * 255.0f); return;
        }

        float q = (l < 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
        float p = 2.0f * l - q;
        float hNorm = h / 360.0f;

        r = clampU8(hue2rgb(p, q, hNorm + 1.0f / 3.0f) * 255.0f);
        g = clampU8(hue2rgb(p, q, hNorm) * 255.0f);
        b = clampU8(hue2rgb(p, q, hNorm - 1.0f / 3.0f) * 255.0f);
    }

    static ColorProfile combineProfiles(const ColorProfile& global, const ColorProfile& specific) {
        ColorProfile res;
        res.deltaR = constrain(global.deltaR + specific.deltaR, -100, 100);
        res.deltaG = constrain(global.deltaG + specific.deltaG, -100, 100);
        res.deltaB = constrain(global.deltaB + specific.deltaB, -100, 100);
        res.brightness = constrain(global.brightness + specific.brightness, -100, 100);
        res.contrast = constrain(global.contrast + specific.contrast, -100, 100);
        res.saturation = constrain(global.saturation + specific.saturation, -100, 100);
        res.swapRB = global.swapRB || specific.swapRB; 
        res.invert = global.invert || specific.invert;
        return res;
    }

    static uint16_t applyProfileToRgb(const ColorProfile& p, uint8_t r, uint8_t g, uint8_t b) {
        if (p.swapRB) { uint8_t tmp = r; r = b; b = tmp; }
        if (p.invert) { r = 255 - r; g = 255 - g; b = 255 - b; }

        if (p.isNeutral() && !p.swapRB && !p.invert) {
            return rgbTo565(r, g, b);
        }

        float h, s, l;
        rgbToHsl(r, g, b, h, s, l);

        if (p.brightness > 0) l += (1.0f - l) * (p.brightness / 100.0f);
        else if (p.brightness < 0) l += l * (p.brightness / 100.0f);
        l = constrain(l, 0.0f, 1.0f);

        if (p.contrast != 0) {
            float cFactor = max(0.0f, 1.0f + ((float)p.contrast / 100.0f));
            l = 0.5f + (l - 0.5f) * cFactor;
            l = constrain(l, 0.0f, 1.0f);
        }

        if (p.saturation > 0) s += (1.0f - s) * (p.saturation / 100.0f);
        else if (p.saturation < 0) s += s * (p.saturation / 100.0f);
        s = constrain(s, 0.0f, 1.0f);

        uint8_t outR, outG, outB;
        hslToRgb(h, s, l, outR, outG, outB);

        uint8_t finalR = clampU8((float)outR + p.deltaR);
        uint8_t finalG = clampU8((float)outG + p.deltaG);
        uint8_t finalB = clampU8((float)outB + p.deltaB);

        return rgbTo565(finalR, finalG, finalB);
    }
}

ColorProfile& getGlobalProfile() { return globalProfile; }

const char* getColorDomainName(ColorDomain domain) {
    if (domain >= DOMAIN_COUNT) return "UNKNOWN";
    return DOMAIN_NAMES[domain];
}

ColorDomain parseColorDomain(const String& name) {
    for (uint8_t i = 0; i < DOMAIN_COUNT; ++i) {
        if (name.equalsIgnoreCase(DOMAIN_NAMES[i])) return (ColorDomain)i;
    }
    return DOMAIN_TAMA;
}

ColorProfile& getColorProfile(ColorDomain domain) {
    if (domain >= DOMAIN_COUNT) return domainProfiles[DOMAIN_TAMA];
    return domainProfiles[domain];
}

void setColorOverride(uint16_t color565, const ColorProfile& prof) { colorOverrides[color565] = prof; }
bool hasColorOverride(uint16_t color565) { return colorOverrides.find(color565) != colorOverrides.end(); }
void removeColorOverride(uint16_t color565) { colorOverrides.erase(color565); }
void clearColorOverrides() { colorOverrides.clear(); }

bool getHardwareBGR() { return currentBGR; }
bool getHardwareInv() { return currentInv; }
bool getHardwareSwapBytes() { return swapPngBytes; }

uint8_t getNightDimPercent() { return nightDimPercent; }
void setNightDimPercent(uint8_t val) { nightDimPercent = constrain(val, 0, 100); }

uint16_t applyNightDim565(uint16_t c565) {
    float mult = 1.0f - ((float)nightDimPercent / 100.0f);
    uint8_t r = ((c565 >> 11) & 0x1F);
    uint8_t g = ((c565 >> 5) & 0x3F);
    uint8_t b = (c565 & 0x1F);
    r = (uint8_t)(r * mult);
    g = (uint8_t)(g * mult);
    b = (uint8_t)(b * mult);
    return (r << 11) | (g << 5) | b;
}

bool colorTunerHasEffect(ColorDomain domain) {
    if (domain >= DOMAIN_COUNT) return false;
    return (!globalProfile.isNeutral() || !domainProfiles[domain].isNeutral() || !colorOverrides.empty() || swapPngBytes);
}

uint16_t colorTunerApply(ColorDomain domain, uint8_t r, uint8_t g, uint8_t b) {
    uint16_t raw565 = rgbTo565(r, g, b);
    auto it = colorOverrides.find(raw565);
    uint16_t c565;
    if (it != colorOverrides.end()) {
        c565 = applyProfileToRgb(combineProfiles(globalProfile, it->second), r, g, b);
    } else {
        if (domain >= DOMAIN_COUNT) domain = DOMAIN_WORLD;
        c565 = applyProfileToRgb(combineProfiles(globalProfile, domainProfiles[domain]), r, g, b);
    }

    if (!previewLightsOn) c565 = applyNightDim565(c565);
    if (swapPngBytes) c565 = __builtin_bswap16(c565);
    return c565;
}

uint16_t colorTunerCorrect565(ColorDomain domain, uint16_t c565) {
    uint16_t key = c565;
    if (swapPngBytes) c565 = __builtin_bswap16(c565);

    uint8_t r = ((c565 >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((c565 >> 5)  & 0x3F) * 255 / 63;
    uint8_t b = (c565 & 0x1F)        * 255 / 31;

    auto it = colorOverrides.find(key);
    uint16_t res;
    if (it != colorOverrides.end()) {
        res = applyProfileToRgb(combineProfiles(globalProfile, it->second), r, g, b);
    } else {
        if (domain >= DOMAIN_COUNT) domain = DOMAIN_WORLD;
        res = applyProfileToRgb(combineProfiles(globalProfile, domainProfiles[domain]), r, g, b);
    }

    if (!previewLightsOn) res = applyNightDim565(res);
    if (swapPngBytes) res = __builtin_bswap16(res);
    return res;
}

void aplicarHardware() {
    if (!_bus) return;
    _bus->beginWrite();
    _bus->writeCommand(0x36);
    _bus->write(currentBGR ? 0x48 : 0x40);
    _bus->writeCommand(currentInv ? 0x21 : 0x20);
    _bus->endWrite();
}

String getRawConfigString() {
    String out = "";
    
    out += "global:bgr=" + String(currentBGR ? 1 : 0) + 
           ";inv=" + String(currentInv ? 1 : 0) + 
           ";swap=" + String(swapPngBytes ? 1 : 0) + 
           ";night=" + String((int)nightDimPercent) + 
           ";dr=" + String((int)globalProfile.deltaR) +
           ";dg=" + String((int)globalProfile.deltaG) +
           ";db=" + String((int)globalProfile.deltaB) +
           ";bri=" + String((int)globalProfile.brightness) +
           ";con=" + String((int)globalProfile.contrast) +
           ";sat=" + String((int)globalProfile.saturation) + "\n";

    for (uint8_t i = 0; i < DOMAIN_COUNT; ++i) {
        const ColorProfile& p = domainProfiles[i];
        out += String(DOMAIN_NAMES[i]) + ":dr=" + String((int)p.deltaR) +
               ";dg=" + String((int)p.deltaG) +
               ";db=" + String((int)p.deltaB) +
               ";bri=" + String((int)p.brightness) +
               ";con=" + String((int)p.contrast) +
               ";sat=" + String((int)p.saturation) +
               ";swapRB=" + String(p.swapRB ? 1 : 0) +
               ";inv=" + String(p.invert ? 1 : 0) + "\n";
    }

    for (const auto& pair : colorOverrides) {
        char hexBuf[10];
        snprintf(hexBuf, sizeof(hexBuf), "0x%04X", pair.first);
        const ColorProfile& p = pair.second;
        out += String(hexBuf) + ":dr=" + String((int)p.deltaR) +
               ";dg=" + String((int)p.deltaG) +
               ";db=" + String((int)p.deltaB) +
               ";bri=" + String((int)p.brightness) +
               ";con=" + String((int)p.contrast) +
               ";sat=" + String((int)p.saturation) +
               ";swapRB=" + String(p.swapRB ? 1 : 0) +
               ";inv=" + String(p.invert ? 1 : 0) + "\n";
    }
    return out;
}

void setRawConfigString(const String& raw) {
    SD_MMC.mkdir("/config");
    File f = SD_MMC.open(CONFIG_PATH, "w");
    if (f) {
        f.print(raw);
        f.close();
        loadColorConfigSD(_bus);
    }
}

String serializeProfilesJson() {
    String json = "{";
    json += "\"bgr\":" + String(currentBGR ? 1 : 0) + ",";
    json += "\"inv\":" + String(currentInv ? 1 : 0) + ",";
    json += "\"swap\":" + String(swapPngBytes ? 1 : 0) + ",";
    json += "\"nightDim\":" + String((int)nightDimPercent) + ",";
    json += "\"lightsOn\":" + String(previewLightsOn ? 1 : 0) + ",";
    
    json += "\"global_profile\":{";
    json += "\"dr\":" + String((int)globalProfile.deltaR) + ",";
    json += "\"dg\":" + String((int)globalProfile.deltaG) + ",";
    json += "\"db\":" + String((int)globalProfile.deltaB) + ",";
    json += "\"bri\":" + String((int)globalProfile.brightness) + ",";
    json += "\"con\":" + String((int)globalProfile.contrast) + ",";
    json += "\"sat\":" + String((int)globalProfile.saturation) + "},";

    json += "\"domains\":{";
    for (uint8_t i = 0; i < DOMAIN_COUNT; ++i) {
        const ColorProfile& p = domainProfiles[i];
        json += "\"" + String(DOMAIN_NAMES[i]) + "\":{";
        json += "\"dr\":" + String((int)p.deltaR) + ",";
        json += "\"dg\":" + String((int)p.deltaG) + ",";
        json += "\"db\":" + String((int)p.deltaB) + ",";
        json += "\"bri\":" + String((int)p.brightness) + ",";
        json += "\"con\":" + String((int)p.contrast) + ",";
        json += "\"sat\":" + String((int)p.saturation) + ",";
        json += "\"swapRB\":" + String(p.swapRB ? 1 : 0) + ",";
        json += "\"inv\":" + String(p.invert ? 1 : 0) + "}";
        if (i < DOMAIN_COUNT - 1) json += ",";
    }
    json += "},";

    json += "\"colors\":{";
    size_t cIdx = 0;
    for (const auto& pair : colorOverrides) {
        char hexBuf[8];
        snprintf(hexBuf, sizeof(hexBuf), "0x%04X", pair.first);
        const ColorProfile& p = pair.second;
        json += "\"" + String(hexBuf) + "\":{";
        json += "\"dr\":" + String((int)p.deltaR) + ",";
        json += "\"dg\":" + String((int)p.deltaG) + ",";
        json += "\"db\":" + String((int)p.deltaB) + ",";
        json += "\"bri\":" + String((int)p.brightness) + ",";
        json += "\"con\":" + String((int)p.contrast) + ",";
        json += "\"sat\":" + String((int)p.saturation) + ",";
        json += "\"swapRB\":" + String(p.swapRB ? 1 : 0) + ",";
        json += "\"inv\":" + String(p.invert ? 1 : 0) + "}";
        if (++cIdx < colorOverrides.size()) json += ",";
    }
    json += "}}";
    return json;
}

void loadColorConfigSD(Arduino_DataBus* bus) {
    if (bus) _bus = bus;
    if (!SD_MMC.exists(CONFIG_PATH)) {
        aplicarHardware(); // Aplica los valores por defecto
        saveColorConfigSD();
        return;
    }

    File f = SD_MMC.open(CONFIG_PATH, "r");
    if (!f) return;

    colorOverrides.clear();

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;

        int colon = line.indexOf(':');
        if (colon == -1) continue;

        String header = line.substring(0, colon);
        String body = line.substring(colon + 1);

        if (header.equalsIgnoreCase("global")) {
            int start = 0;
            while (start < body.length()) {
                int semi = body.indexOf(';', start);
                if (semi == -1) semi = body.length();
                String token = body.substring(start, semi);
                int eq = token.indexOf('=');
                if (eq != -1) {
                    String k = token.substring(0, eq);
                    String v = token.substring(eq + 1);
                    if (k.equalsIgnoreCase("bgr")) currentBGR = (v.toInt() == 1);
                    else if (k.equalsIgnoreCase("inv")) currentInv = (v.toInt() == 1);
                    else if (k.equalsIgnoreCase("swap")) swapPngBytes = (v.toInt() == 1);
                    else if (k.equalsIgnoreCase("night")) nightDimPercent = constrain(v.toInt(), 0, 100);
                    else if (k.equalsIgnoreCase("dr")) globalProfile.deltaR = constrain(v.toInt(), -100, 100);
                    else if (k.equalsIgnoreCase("dg")) globalProfile.deltaG = constrain(v.toInt(), -100, 100);
                    else if (k.equalsIgnoreCase("db")) globalProfile.deltaB = constrain(v.toInt(), -100, 100);
                    else if (k.equalsIgnoreCase("bri")) globalProfile.brightness = constrain(v.toInt(), -100, 100);
                    else if (k.equalsIgnoreCase("con")) globalProfile.contrast = constrain(v.toInt(), -100, 100);
                    else if (k.equalsIgnoreCase("sat")) globalProfile.saturation = constrain(v.toInt(), -100, 100);
                }
                start = semi + 1;
            }
        } else if (header.startsWith("0x") || header.startsWith("0X")) {
            uint16_t cKey = (uint16_t)strtoul(header.c_str(), NULL, 16);
            ColorProfile p;

            int start = 0;
            while (start < body.length()) {
                int semi = body.indexOf(';', start);
                if (semi == -1) semi = body.length();
                String token = body.substring(start, semi);
                int eq = token.indexOf('=');
                if (eq != -1) {
                    String k = token.substring(0, eq);
                    int val = token.substring(eq + 1).toInt();
                    if (k.equalsIgnoreCase("dr")) p.deltaR = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("dg")) p.deltaG = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("db")) p.deltaB = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("bri")) p.brightness = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("con")) p.contrast = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("sat")) p.saturation = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("swapRB")) p.swapRB = (val == 1);
                    else if (k.equalsIgnoreCase("inv")) p.invert = (val == 1);
                }
                start = semi + 1;
            }
            colorOverrides[cKey] = p;
        } else {
            ColorDomain d = parseColorDomain(header);
            ColorProfile& p = domainProfiles[d];

            int start = 0;
            while (start < body.length()) {
                int semi = body.indexOf(';', start);
                if (semi == -1) semi = body.length();
                String token = body.substring(start, semi);
                int eq = token.indexOf('=');
                if (eq != -1) {
                    String k = token.substring(0, eq);
                    int val = token.substring(eq + 1).toInt();
                    if (k.equalsIgnoreCase("dr")) p.deltaR = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("dg")) p.deltaG = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("db")) p.deltaB = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("bri")) p.brightness = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("con")) p.contrast = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("sat")) p.saturation = constrain(val, -100, 100);
                    else if (k.equalsIgnoreCase("swapRB")) p.swapRB = (val == 1);
                    else if (k.equalsIgnoreCase("inv")) p.invert = (val == 1);
                }
                start = semi + 1;
            }
        }
    }
    f.close();
    aplicarHardware();
}

void saveColorConfigSD() {
    SD_MMC.mkdir("/config");
    File f = SD_MMC.open(CONFIG_PATH, "w");
    if (f) {
        f.print(getRawConfigString());
        f.close();
        Serial.println("[COLOR-SD] Configuracion guardada exitosamente.");
    }
}

void factoryResetColorSD(Arduino_DataBus* bus) {
    if (bus) _bus = bus;
    if (SD_MMC.exists(CONFIG_PATH)) SD_MMC.remove(CONFIG_PATH);

    currentBGR = true;
    currentInv = false;
    swapPngBytes = false;
    nightDimPercent = 75;
    previewLightsOn = true;
    
    globalProfile = ColorProfile(0, 0, 0, 40, 30, 0, false, false); 
    
    domainProfiles[DOMAIN_TAMA] = ColorProfile(0, 0, 0, 0, 0, 0, false, false);
    domainProfiles[DOMAIN_WORLD] = ColorProfile(0, 0, 0, 0, 0, 0, false, false);
    domainProfiles[DOMAIN_BUTTONS] = ColorProfile(0, 0, 0, -15, -11, 0, false, false);
    domainProfiles[DOMAIN_HUD] = ColorProfile(0, 0, 0, -40, -30, 0, false, false);
    domainProfiles[DOMAIN_THOUGHT] = ColorProfile(0, 0, 0, 0, 0, 0, false, false);
    domainProfiles[DOMAIN_POPUP_DAY] = ColorProfile(0, 0, 0, -40, -30, 0, false, false);
    domainProfiles[DOMAIN_POPUP_DREAM] = ColorProfile(0, 0, 0, 0, 0, 0, false, false);

    colorOverrides.clear();
    domainProfiles[DOMAIN_PLAZA] = ColorProfile(0, 0, 0, -40, -30, 0, false, false);
    aplicarHardware();
}

bool handleColorTunerCommand(const String& cmdRaw, Arduino_DataBus* bus) {
    if (bus) _bus = bus;
    String cmd = cmdRaw;
    cmd.trim();
    if (cmd.length() == 0) return false;

    static bool isReceivingRaw = false;
    static String rawBuffer = "";

    if (cmd == "!SET_RAW_START") {
        isReceivingRaw = true;
        rawBuffer = "";
        return true;
    }
    if (cmd == "!SET_RAW_END") {
        isReceivingRaw = false;
        setRawConfigString(rawBuffer);
        tunerDirty = true;
        Serial.println("[TUNER] Configuración RAW recibida y aplicada.");
        return true;
    }
    if (isReceivingRaw) {
        rawBuffer += cmd + "\n";
        return true;
    }

    if (cmd == "TUNECOLOURS") {
        tunerActive = true;
        tunerDirty = true;
        Serial.println("[TUNER] MODO CALIBRADOR EN EJECUCIÓN");
        Serial.println("!PROFILES:" + serializeProfilesJson());
        return true;
    }
    if (cmd == "GET_PROFILES") {
        Serial.println("!PROFILES:" + serializeProfilesJson());
        return true;
    }
    if (cmd == "GET_RAW_CFG") {
        Serial.println("!RAW_CFG_START");
        Serial.print(getRawConfigString());
        Serial.println("!RAW_CFG_END");
        return true;
    }
    if (cmd == "SAVE") {
        saveColorConfigSD();
        Serial.println("[TUNER] Guardado exitoso en SD.");
        return true;
    }
    if (cmd == "FACTORY") {
        factoryResetColorSD(_bus);
        tunerDirty = true;
        Serial.println("[TUNER] Valores restaurados de fábrica.");
        Serial.println("!PROFILES:" + serializeProfilesJson());
        return true;
    }
    if (cmd.startsWith("BASE ")) {
        int p1 = cmd.indexOf(' ');
        int p2 = cmd.indexOf(' ', p1 + 1);
        int p3 = cmd.indexOf(' ', p2 + 1);
        if (p3 != -1) {
            curBaseR = (uint8_t)cmd.substring(p1 + 1, p2).toInt();
            curBaseG = (uint8_t)cmd.substring(p2 + 1, p3).toInt();
            curBaseB = (uint8_t)cmd.substring(p3 + 1).toInt();
            tunerDirty = true;
        }
        return true;
    }
    if (cmd.startsWith("DOMAIN ")) {
        String dom = cmd.substring(7);
        dom.trim();
        if (dom.equalsIgnoreCase("GLOBAL")) {
            // El dominio GLOBAL se visualiza sobre la pantalla principal pausada
            activeTunerDomain = DOMAIN_TAMA;
        } else {
            activeTunerDomain = parseColorDomain(dom);
        }
        Serial.printf("[TUNER] Dominio activo: %s\n", getColorDomainName(activeTunerDomain));
        tunerDirty = true;
        return true;
    }
    if (cmd.startsWith("TRIM ")) {
        int p1 = cmd.indexOf(' ');
        int p2 = cmd.indexOf(' ', p1 + 1);
        int p3 = cmd.indexOf(' ', p2 + 1);
        int p4 = cmd.indexOf(' ', p3 + 1);
        int p5 = cmd.indexOf(' ', p4 + 1);
        int p6 = cmd.indexOf(' ', p5 + 1);
        int p7 = cmd.indexOf(' ', p6 + 1);
        if (p7 != -1) {
            String domName = cmd.substring(p1 + 1, p2);
            ColorDomain d = parseColorDomain(domName);
            ColorProfile& p = domainProfiles[d];
            p.deltaR     = constrain(cmd.substring(p2 + 1, p3).toInt(), -100, 100);
            p.deltaG     = constrain(cmd.substring(p3 + 1, p4).toInt(), -100, 100);
            p.deltaB     = constrain(cmd.substring(p4 + 1, p5).toInt(), -100, 100);
            p.brightness = constrain(cmd.substring(p5 + 1, p6).toInt(), -100, 100);
            p.contrast   = constrain(cmd.substring(p6 + 1, p7).toInt(), -100, 100);
            p.saturation = constrain(cmd.substring(p7 + 1).toInt(), -100, 100);
            tunerDirty = true;
            return true;
        }
    }
    if (cmd.startsWith("GLOBAL_TRIM ")) {
        int p1 = cmd.indexOf(' ');
        int p2 = cmd.indexOf(' ', p1 + 1);
        int p3 = cmd.indexOf(' ', p2 + 1);
        int p4 = cmd.indexOf(' ', p3 + 1);
        int p5 = cmd.indexOf(' ', p4 + 1);
        int p6 = cmd.indexOf(' ', p5 + 1);
        if (p6 != -1) {
            globalProfile.deltaR     = constrain(cmd.substring(p1 + 1, p2).toInt(), -100, 100);
            globalProfile.deltaG     = constrain(cmd.substring(p2 + 1, p3).toInt(), -100, 100);
            globalProfile.deltaB     = constrain(cmd.substring(p3 + 1, p4).toInt(), -100, 100);
            globalProfile.brightness = constrain(cmd.substring(p4 + 1, p5).toInt(), -100, 100);
            globalProfile.contrast   = constrain(cmd.substring(p5 + 1, p6).toInt(), -100, 100);
            globalProfile.saturation = constrain(cmd.substring(p6 + 1).toInt(), -100, 100);
            tunerDirty = true;
            return true;
        }
    }
    if (cmd.startsWith("COLOR ")) {
        int p1 = cmd.indexOf(' ');
        int p2 = cmd.indexOf(' ', p1 + 1);
        int p3 = cmd.indexOf(' ', p2 + 1);
        int p4 = cmd.indexOf(' ', p3 + 1);
        int p5 = cmd.indexOf(' ', p4 + 1);
        int p6 = cmd.indexOf(' ', p5 + 1);
        int p7 = cmd.indexOf(' ', p6 + 1);
        if (p7 != -1) {
            String hexStr = cmd.substring(p1 + 1, p2);
            uint16_t cKey = (uint16_t)strtoul(hexStr.c_str(), NULL, 16);
            ColorProfile p;
            p.deltaR     = constrain(cmd.substring(p2 + 1, p3).toInt(), -100, 100);
            p.deltaG     = constrain(cmd.substring(p3 + 1, p4).toInt(), -100, 100);
            p.deltaB     = constrain(cmd.substring(p4 + 1, p5).toInt(), -100, 100);
            p.brightness = constrain(cmd.substring(p5 + 1, p6).toInt(), -100, 100);
            p.contrast   = constrain(cmd.substring(p6 + 1, p7).toInt(), -100, 100);
            p.saturation = constrain(cmd.substring(p7 + 1).toInt(), -100, 100);
            colorOverrides[cKey] = p;
            tunerDirty = true;
            return true;
        }
    }
    if (cmd.startsWith("COLOR_RESET ")) {
        String hexStr = cmd.substring(12);
        hexStr.trim();
        uint16_t cKey = (uint16_t)strtoul(hexStr.c_str(), NULL, 16);
        colorOverrides.erase(cKey);
        tunerDirty = true;
        return true;
    }
    if (cmd == "RGB") { currentBGR = false; aplicarHardware(); tunerDirty = true; return true; }
    if (cmd == "BGR") { currentBGR = true; aplicarHardware(); tunerDirty = true; return true; }
    if (cmd == "SWAP") { swapPngBytes = !swapPngBytes; tunerDirty = true; return true; }
    if (cmd == "INV ON") { currentInv = true; aplicarHardware(); tunerDirty = true; return true; }
    if (cmd == "INV OFF") { currentInv = false; aplicarHardware(); tunerDirty = true; return true; }
    if (cmd == "EXIT") {
        tunerActive = false;
        tunerDirty = false;
        Serial.println("[TUNER] Saliendo del calibrador hacia TAMA...");
        return true;
    }
    return false;
}

bool isColorTunerActive() { return tunerActive; }
void setColorTunerActive(bool active) { tunerActive = active; tunerDirty = true; }

void renderColorTunerScreen(uint16_t* fb, uint16_t width, uint16_t height, Arduino_GFX* gfx) {
    if (!fb || !gfx || !tunerActive || !tunerDirty) return;
    tunerDirty = false;

    // Limpiar caché para que los assets se decodifiquen con el perfil de color recién ajustado
    AssetManager::getInstance().clearAll();

    RenderContext ctx(width, height, fb);

    if (activeTunerDomain == DOMAIN_PLAZA) {
        // ================= VISTA 1: LA PLAZA PURA (SIN BICHOS) =================
        auto plazaTex = AssetManager::getInstance().getTexture("/tama/ui/Plaza/bg_plaza.png");
        if (plazaTex && plazaTex->pixels) {
            int16_t texW = plazaTex->width;
            int16_t texH = plazaTex->height;
            int16_t startSrcX = (texW > width) ? (texW - width) / 2 : 0;

            for (int16_t y = 0; y < height; ++y) {
                int16_t srcY = (y * texH) / height;
                if (srcY >= texH) srcY = texH - 1;
                uint16_t* srcRow = &plazaTex->pixels[srcY * texW];
                uint16_t* dstRow = &fb[y * width];
                for (int16_t x = 0; x < width; ++x) {
                    int16_t srcX = startSrcX + x;
                    if (srcX >= texW) srcX = texW - 1;
                    dstRow[x] = srcRow[srcX];
                }
            }
        } else {
            // Fallback: Mosaico procedural pastel con DOMAIN_PLAZA
            static const uint16_t tileColors[4] = { 0xEF7D, 0xE79C, 0xDF7E, 0xF79B };
            uint16_t cGrout = colorTunerCorrect565(DOMAIN_PLAZA, 0xD6BA);
            uint16_t cTiles[4];
            for (int i = 0; i < 4; ++i) cTiles[i] = colorTunerCorrect565(DOMAIN_PLAZA, tileColors[i]);

            for (int16_t y = 0; y < height; ++y) {
                int16_t yShift = (y >> 3);
                uint16_t* rowPtr = &fb[y * width];
                for (int16_t x = 0; x < width; ++x) {
                    int16_t u = x + yShift;
                    int16_t v = y - (x >> 3);
                    int16_t modU = ((u % 22) + 22) % 22;
                    int16_t modV = ((v % 22) + 22) % 22;
                    uint16_t c = cGrout;
                    if (modU >= 2 && modV >= 2) {
                        c = cTiles[((u / 22) + (v / 22)) & 3];
                    }
                    rowPtr[x] = c;
                }
            }
        }

        // Iconos superiores minimalistas (Flecha volver y Silbato Wii)
        uint16_t cIconCol = colorTunerCorrect565(DOMAIN_HUD, 0x4903);
        int16_t r = 12, cx = 8 + r, cy = 8 + r;
        for (int16_t y = -r; y <= r; ++y) {
            for (int16_t x = -r; x <= r; ++x) {
                if (x * x + y * y <= r * r) ctx.drawPixel(cx + x, cy + y, cIconCol);
            }
        }
        TextUtil::drawWrappedText(ctx, cx - 3, cy - 4, "<", 0xFFFF, 12, 10);

        int16_t wx = width - 30, wy = 8;
        for (int16_t y = 0; y < 22; ++y) {
            for (int16_t x = 0; x < 22; ++x) {
                bool isBody = ((x >= 7 && x <= 18 && y >= 7 && y <= 18) || (x >= 2 && x <= 9 && y >= 10 && y <= 15));
                if (isBody) ctx.drawPixel(wx + x, wy + y, cIconCol);
            }
        }
        ctx.drawPixel(wx + 11, wy + 12, 0xFFFF);

    } else {
        // ================= VISTA 2: PANTALLA PRINCIPAL PAUSADA (FRAME NORMAL) =================
        // 1. Fondo de la habitación (bg_main.png)
        auto bgTex = AssetManager::getInstance().getTexture("/tama/ui/bg_main.png");
        if (bgTex && bgTex->pixels) {
            int16_t texW = bgTex->width;
            int16_t texH = bgTex->height;
            int16_t scaleX = (texW < width) ? 2 : 1;
            int16_t scaleY = (texH < height) ? 2 : 1;

            for (int16_t j = 0; j < texH; ++j) {
                int16_t destY = j * scaleY;
                for (int16_t sy = 0; sy < scaleY; ++sy) {
                    int16_t fy = destY + sy;
                    if (fy >= height) continue;
                    for (int16_t i = 0; i < texW; ++i) {
                        int16_t destX = i * scaleX;
                        uint16_t color = bgTex->pixels[j * texW + i];
                        for (int16_t sx = 0; sx < scaleX; ++sx) {
                            int16_t fx = destX + sx;
                            if (fx < width) fb[fy * width + fx] = color;
                        }
                    }
                }
            }
        } else {
            for (uint32_t i = 0; i < (uint32_t)(width * height); ++i) fb[i] = 0x18C3;
        }

        // 2. Tamagotchi en el centro a escala real 3x (144x144 px)
        auto petTex = AssetManager::getInstance().getTexture("/tama/sprites/base/tiernito/child/idle.png");
        if (!petTex) petTex = AssetManager::getInstance().getTexture("/tama/sprites/base/tiernito/bebe/idle.png");
        if (petTex) {
            Sprite petSpr(petTex);
            petSpr.frameSize = {48, 48};
            petSpr.frameOffset = {0, 0};
            petSpr.scale = {3, 3};
            petSpr.position = { (int16_t)((width - 144) / 2), (int16_t)((height - 144) / 2) };
            petSpr.draw(ctx);
        }

        // 3. Botones inferiores usuales en Y = 275 (Comida, Medicina, Limpieza, Luz)
        auto iconsTex = AssetManager::getInstance().getTexture("/tama/ui/icons_ui.png");
        if (iconsTex) {
            Sprite btn(iconsTex);
            btn.frameSize = {16, 16};
            btn.scale = {2, 2};

            btn.frameOffset = {16, 0};  btn.position = {10, 275};  btn.draw(ctx);
            btn.frameOffset = {0, 0};   btn.position = {52, 275};  btn.draw(ctx);
            btn.frameOffset = {0, 16};  btn.position = {94, 275};  btn.draw(ctx);
            btn.frameOffset = {16, 16}; btn.position = {136, 275}; btn.draw(ctx);
        }

        // 4. Barras HUD superiores (Hambre 85%, Felicidad 70%, Energía 95%)
        int16_t hudX = 6, hudY = 6;
        auto drawHudBar = [&](int16_t y, float val, uint16_t barCol, int iconType) {
            static const uint8_t iconHeart[5] = {0x0A, 0x1F, 0x1F, 0x0E, 0x04};
            static const uint8_t iconStar[5]  = {0x04, 0x15, 0x0E, 0x15, 0x04};
            static const uint8_t iconBolt[5]  = {0x06, 0x0C, 0x1F, 0x06, 0x0C};
            const uint8_t* idata = (iconType == 0) ? iconHeart : (iconType == 1 ? iconStar : iconBolt);
            uint16_t iconCol = colorTunerCorrect565(DOMAIN_HUD, (iconType == 0) ? 0xF800 : (iconType == 1 ? 0xFEE0 : 0x07FF));

            for (int r = 0; r < 5; ++r) {
                uint8_t bits = idata[r];
                for (int c = 0; c < 5; ++c) {
                    if (bits & (1 << (4 - c))) ctx.drawPixel(hudX + c, y + 1 + r, iconCol);
                }
            }

            int16_t bx = hudX + 8, barW = 68, barH = 7;
            uint16_t cBorder = colorTunerCorrect565(DOMAIN_HUD, 0x2104);
            uint16_t cEmpty  = colorTunerCorrect565(DOMAIN_HUD, 0x10A2);
            uint16_t cFill   = colorTunerCorrect565(DOMAIN_HUD, barCol);

            for (int16_t x = 0; x < barW; ++x) {
                ctx.drawPixel(bx + x, y, cBorder);
                ctx.drawPixel(bx + x, y + barH - 1, cBorder);
            }
            for (int16_t py = 0; py < barH; ++py) {
                ctx.drawPixel(bx, y + py, cBorder);
                ctx.drawPixel(bx + barW - 1, y + py, cBorder);
            }
            int16_t fillW = (int16_t)((val * (barW - 2)) / 100.0f);
            for (int16_t py = 1; py < barH - 1; ++py) {
                for (int16_t px = 1; px < barW - 1; ++px) {
                    ctx.drawPixel(bx + px, y + py, (px <= fillW) ? cFill : cEmpty);
                }
            }
        };

        drawHudBar(hudY,      85.0f, 0xFA60, 0);
        drawHudBar(hudY + 8,  70.0f, 0xFDE0, 1);
        drawHudBar(hudY + 16, 95.0f, 0x05BF, 2);

        // Reloj superior
        uint16_t cClockBorder = colorTunerCorrect565(DOMAIN_HUD, 0x2104);
        uint16_t cClockBg     = colorTunerCorrect565(DOMAIN_HUD, 0x4903);
        for (int16_t py = 0; py < 18; ++py) {
            for (int16_t px = 0; px < 54; ++px) {
                uint16_t c = (px == 0 || py == 0 || px == 53 || py == 17) ? cClockBorder : cClockBg;
                ctx.drawPixel(110 + px, 8 + py, c);
            }
        }
        TextUtil::drawWrappedText(ctx, 122, 13, "12:00", 0xFFEC, 40, 10);

        // 5. Globo de pensamiento si estamos calibrando THOUGHT
        if (activeTunerDomain == DOMAIN_THOUGHT) {
            int16_t tbx = 68, tby = 78;
            uint16_t cBorder = colorTunerCorrect565(DOMAIN_THOUGHT, 0x2104);
            uint16_t cBg     = colorTunerCorrect565(DOMAIN_THOUGHT, 0xFFFF);
            for (int y = 0; y < 22; ++y) {
                for (int x = 0; x < 32; ++x) {
                    if ((x <= 1 && y <= 1) || (x >= 30 && y <= 1) || (x <= 1 && y >= 20) || (x >= 30 && y >= 20)) continue;
                    bool isB = (x == 0 || x == 31 || y == 0 || y == 21 || (x <= 2 && y <= 2) || (x >= 29 && y <= 2) || (x <= 2 && y >= 19) || (x >= 29 && y >= 19));
                    ctx.drawPixel(tbx + x, tby + y, isB ? cBorder : cBg);
                }
            }
            uint16_t cHeart = colorTunerCorrect565(DOMAIN_THOUGHT, 0xF800);
            for (int16_t y = 0; y < 7; ++y) {
                for (int16_t x = 0; x < 9; ++x) ctx.drawPixel(tbx + 12 + x, tby + 8 + y, cHeart);
            }
        }

        // 6. Modal de diálogo si estamos calibrando POPUP_DAY o POPUP_DREAM
        if (activeTunerDomain == DOMAIN_POPUP_DAY || activeTunerDomain == DOMAIN_POPUP_DREAM) {
            ColorDomain pDomain = activeTunerDomain;
            int16_t px = 10, py = 80, pw = 152, ph = 150;
            uint16_t cBorderOut = colorTunerCorrect565(pDomain, 0x4903);
            uint16_t cBorderIn  = colorTunerCorrect565(pDomain, 0xFFFF);
            uint16_t cBody      = colorTunerCorrect565(pDomain, (pDomain == DOMAIN_POPUP_DREAM) ? 0x2965 : 0xF6FA);
            uint16_t cText      = colorTunerCorrect565(pDomain, (pDomain == DOMAIN_POPUP_DREAM) ? 0xDEFB : 0x4903);

            for (int16_t y = 0; y < ph; ++y) {
                for (int16_t x = 0; x < pw; ++x) {
                    uint16_t c = cBody;
                    if (x < 2 || x >= pw - 2 || y < 2 || y >= ph - 2) c = cBorderOut;
                    else if (x == 2 || x == pw - 3 || y == 2 || y == ph - 3) c = cBorderIn;
                    ctx.drawPixel(px + x, py + y, c);
                }
            }
            TextUtil::drawWrappedText(ctx, px + 12, py + 16, "CALIBRANDO POPUP", cText, pw - 24, 12);
            uint16_t btnCol = colorTunerCorrect565(pDomain, 0x54A8);
            for (int16_t y = 0; y < 32; ++y) {
                for (int16_t x = 0; x < pw - 24; ++x) {
                    ctx.drawPixel(px + 12 + x, py + 48 + y, btnCol);
                }
            }
            TextUtil::drawWrappedText(ctx, px + 40, py + 58, "ACEPTAR", 0xFFFF, 80, 12);
        }
    }

    // Etiqueta indicadora sutil en la parte inferior
    String tag = "[" + String(getColorDomainName(activeTunerDomain)) + "]";
    TextUtil::drawWrappedText(ctx, 4, height - 12, tag.c_str(), 0xFFFF, 80, 10);
    TextUtil::drawWrappedText(ctx, width - 78, height - 12, "SALIR EN PC", 0xFEE0, 76, 10);

    gfx->draw16bitRGBBitmap(0, 0, fb, width, height);
}
