// src/ColorTuner.cpp
#include "ColorTuner.h"
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
        ColorProfile(0, 0, 0, 0, 0, 0, false, false)         // DOMAIN_POPUP_DREAM
    };

    static std::unordered_map<uint16_t, ColorProfile> colorOverrides;
    static ColorDomain activeTunerDomain = DOMAIN_TAMA;

    static const char* DOMAIN_NAMES[DOMAIN_COUNT] = {
        "TAMA", "WORLD", "BUTTONS", "HUD", "THOUGHT", "POPUP_DAY", "POPUP_DREAM"
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
    aplicarHardware();
}
// ================= BUCLE PRINCIPAL DEL TUNER (borrado) =================

