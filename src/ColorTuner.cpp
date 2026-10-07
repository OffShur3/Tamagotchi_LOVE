// src/ColorTuner.cpp
#include "ColorTuner.h"
#include <SD_MMC.h>
#include <unordered_map>
#include <math.h>

namespace {
    // Dominio y color activo
    static ColorDomain activeTunerDomain = DOMAIN_TAMA;
    static uint8_t baseR = 220, baseG = 20, baseB = 20;

    // --- NUEVO: Perfil Global ---
    static ColorProfile globalProfile;

    // --- NUEVO: Función para combinar perfiles ---
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
    static Arduino_GFX* _gfx = nullptr;
    static Arduino_DataBus* _bus = nullptr;
    static PNG* _png = nullptr;

    static uint16_t lineBuffer[320];
    static uint16_t* bgCache = nullptr;
    static File pngFile;

    const char* CONFIG_PATH = "/config/color_tuning.txt";

    // Registros de Hardware Globales
    static bool currentBGR = true;
    static bool currentInv = false;
    static bool swapPngBytes = false;

    // Atenuación nocturna
    static uint8_t nightDimPercent = 75;
    static bool previewLightsOn = true;

    // Perfiles por Dominio
    static ColorProfile domainProfiles[DOMAIN_COUNT];

    // Overrides por color específico (RGB565)
    static std::unordered_map<uint16_t, ColorProfile> colorOverrides;

    // Dominio y color activo
    static ColorDomain activeTunerDomain = DOMAIN_TAMA;
    static uint8_t baseR = 220, baseG = 20, baseB = 20;

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

    // --- CONVERSIÓN HSL DESACOPLADA ---
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

ColorProfile& getGlobalProfile() { return globalProfile; }

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

uint16_t colorTunerApply(ColorDomain domain, uint8_t r, uint8_t g, uint8_t b) {
    uint16_t raw565 = rgbTo565(r, g, b);
    auto it = colorOverrides.find(raw565);
    uint16_t c565;
    if (it != colorOverrides.end()) {
        c565 = applyProfileToRgb(it->second, r, g, b);
    } else {
        if (domain >= DOMAIN_COUNT) domain = DOMAIN_WORLD;
        c565 = applyProfileToRgb(domainProfiles[domain], r, g, b);
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
        res = applyProfileToRgb(it->second, r, g, b);
    } else {
        if (domain >= DOMAIN_COUNT) domain = DOMAIN_WORLD;
        res = applyProfileToRgb(domainProfiles[domain], r, g, b);
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

static void* tunerPngOpen(const char *filename, int32_t *size) {
    pngFile = SD_MMC.open(filename, "r");
    if (!pngFile || pngFile.isDirectory()) return nullptr;
    *size = pngFile.size();
    return &pngFile;
}
static void tunerPngClose(void *handle) { if (pngFile) pngFile.close(); }
static int32_t tunerPngRead(PNGFILE *handle, uint8_t *buffer, int32_t length) { return pngFile.read(buffer, length); }
static int32_t tunerPngSeek(PNGFILE *handle, int32_t position) { return pngFile.seek(position) ? position : -1; }

static int currentPngDestX = 0, currentPngDestY = 0, currentPngScale = 1;
static ColorDomain currentPngDomain = DOMAIN_WORLD;

static int tunerPngDrawGeneric(PNGDRAW *pDraw) {
    int lineY = currentPngDestY + (pDraw->y * currentPngScale);
    if (lineY < 0 || lineY >= 320) return 1;

    if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
        uint8_t* src = (uint8_t*)pDraw->pPixels;
        for (int x = 0; x < pDraw->iWidth; ++x) {
            uint8_t a = src[3];
            if (a > 32) {
                uint16_t c = colorTunerApply(currentPngDomain, src[0], src[1], src[2]);
                int startX = currentPngDestX + (x * currentPngScale);
                for (int sy = 0; sy < currentPngScale; ++sy) {
                    int py = lineY + sy;
                    if (py >= 0 && py < 320) {
                        for (int sx = 0; sx < currentPngScale; ++sx) {
                            int px = startX + sx;
                            if (px >= 0 && px < 172) {
                                _gfx->drawPixel(px, py, c);
                                if (bgCache && currentPngDomain == DOMAIN_WORLD) {
                                    bgCache[py * 172 + px] = c;
                                }
                            }
                        }
                    }
                }
            }
            src += 4;
        }
    } else {
        _png->getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0);
        for (int x = 0; x < pDraw->iWidth; ++x) {
            uint16_t c = colorTunerCorrect565(currentPngDomain, lineBuffer[x]);
            int startX = currentPngDestX + (x * currentPngScale);
            for (int sy = 0; sy < currentPngScale; ++sy) {
                int py = lineY + sy;
                if (py >= 0 && py < 320) {
                    for (int sx = 0; sx < currentPngScale; ++sx) {
                        int px = startX + sx;
                        if (px >= 0 && px < 172) {
                            _gfx->drawPixel(px, py, c);
                            if (bgCache && currentPngDomain == DOMAIN_WORLD) {
                                bgCache[py * 172 + px] = c;
                            }
                        }
                    }
                }
            }
        }
    }
    return 1;
}

// ================= REDIBUJADO PARCIAL Y TOTAL =================

static void redibujarHud() {
    if (!_gfx) return;

    int boxX = 6, boxY = 6, boxW = 160, boxH = 68;
    uint16_t cBoxBg = 0x1082;
    uint16_t cBorder = 0x4903;
    uint16_t cWhite = 0xFFFF;
    uint16_t cGold  = 0xFEE0;

    _gfx->drawRect(boxX, boxY, boxW, boxH, cBorder);
    _gfx->drawRect(boxX + 1, boxY + 1, boxW - 2, boxH - 2, cWhite);
    _gfx->fillRect(boxX + 2, boxY + 2, boxW - 4, boxH - 4, cBoxBg);

    uint16_t colOriginal = rgbTo565(baseR, baseG, baseB);
    uint16_t colAjustado = colorTunerApply(activeTunerDomain, baseR, baseG, baseB);

    _gfx->drawRect(boxX + 6, boxY + 6, 28, 20, cWhite);
    _gfx->fillRect(boxX + 7, boxY + 7, 26, 18, colOriginal);

    _gfx->drawRect(boxX + 38, boxY + 6, 28, 20, cWhite);
    _gfx->fillRect(boxX + 39, boxY + 7, 26, 18, colAjustado);

    _gfx->setTextSize(1);
    _gfx->setTextColor(cGold);
    _gfx->setCursor(boxX + 70, boxY + 8);
    _gfx->printf("[%s]", DOMAIN_NAMES[activeTunerDomain]);

    _gfx->setTextColor(previewLightsOn ? 0x07E0 : 0x03FF);
    _gfx->setCursor(boxX + 70, boxY + 18);
    _gfx->printf("LUZ: %s (%d%%)", previewLightsOn ? "ON" : "OFF", nightDimPercent);

    const ColorProfile& cp = domainProfiles[activeTunerDomain];
    _gfx->setTextColor(cWhite);
    _gfx->setCursor(boxX + 6, boxY + 30);
    _gfx->printf("dR:%+d dG:%+d dB:%+d", cp.deltaR, cp.deltaG, cp.deltaB);

    _gfx->setCursor(boxX + 6, boxY + 41);
    _gfx->printf("Bri:%+d Con:%+d Sat:%+d", cp.brightness, cp.contrast, cp.saturation);

    _gfx->setCursor(boxX + 6, boxY + 52);
    _gfx->printf("565: 0x%04X -> 0x%04X", colOriginal, colAjustado);
}

static void redibujarTama() {
    if (!_gfx) return;

    int petDestX = (172 - 144) / 2;
    int petDestY = (320 - 144) / 2 + 10;
    int petW = 144, petH = 144;

    if (bgCache) {
        for (int y = 0; y < petH; ++y) {
            int py = petDestY + y;
            if (py >= 0 && py < 320) {
                _gfx->draw16bitRGBBitmap(petDestX, py, &bgCache[py * 172 + petDestX], petW, 1);
            }
        }
    } else {
        _gfx->fillRect(petDestX, petDestY, petW, petH, 0x18C3);
    }

    const char* petPath = "/tama/sprites/base/tiernito/bebe/idle.png";
    if (SD_MMC.exists(petPath)) {
        currentPngDestX = petDestX;
        currentPngDestY = petDestY;
        currentPngScale = 3;
        currentPngDomain = DOMAIN_TAMA;
        if (_png->open(petPath, tunerPngOpen, tunerPngClose, tunerPngRead, tunerPngSeek, tunerPngDrawGeneric) == PNG_SUCCESS) {
            _png->decode(NULL, 0);
            _png->close();
        }
    }
    redibujarHud();
}

static void redibujarTodo() {
    if (!_gfx) return;

    if (!bgCache) {
        bgCache = (uint16_t*)heap_caps_malloc(172 * 320 * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!bgCache) bgCache = (uint16_t*)malloc(172 * 320 * sizeof(uint16_t));
    }

    if (SD_MMC.exists("/tama/ui/bg_main.png")) {
        currentPngDestX = 0; currentPngDestY = 0; currentPngScale = 1; currentPngDomain = DOMAIN_WORLD;
        if (_png->open("/tama/ui/bg_main.png", tunerPngOpen, tunerPngClose, tunerPngRead, tunerPngSeek, tunerPngDrawGeneric) == PNG_SUCCESS) {
            _png->decode(NULL, 0);
            _png->close();
        }
    } else {
        _gfx->fillScreen(0x18C3);
        if (bgCache) {
            for (int i = 0; i < 172 * 320; ++i) bgCache[i] = 0x18C3;
        }
    }

    const char* petPath = "/tama/sprites/base/tiernito/bebe/idle.png";
    if (SD_MMC.exists(petPath)) {
        currentPngDestX = (172 - 144) / 2;
        currentPngDestY = (320 - 144) / 2 + 10;
        currentPngScale = 3;
        currentPngDomain = DOMAIN_TAMA;
        if (_png->open(petPath, tunerPngOpen, tunerPngClose, tunerPngRead, tunerPngSeek, tunerPngDrawGeneric) == PNG_SUCCESS) {
            _png->decode(NULL, 0);
            _png->close();
        }
    }

    if (SD_MMC.exists("/tama/ui/icons_ui.png")) {
        currentPngDestX = 14; currentPngDestY = 275; currentPngScale = 2; currentPngDomain = DOMAIN_BUTTONS;
        if (_png->open("/tama/ui/icons_ui.png", tunerPngOpen, tunerPngClose, tunerPngRead, tunerPngSeek, tunerPngDrawGeneric) == PNG_SUCCESS) {
            _png->decode(NULL, 0);
            _png->close();
        }
    }

    redibujarHud();
}

// ================= SERIALIZACIÓN / PERSISTENCIA EN SD =================

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
    json += "\"domains\":{";
    for (uint8_t i = 0; i < DOMAIN_COUNT; ++i) {
        const ColorProfile& p = domainProfiles[i];
        json += "\"" + String(DOMAIN_NAMES[i]) + "\":{";
        json += "\"global_profile\":{";
        json += "\"dr\":" + String((int)globalProfile.deltaR) + ",";
        json += "\"dg\":" + String((int)globalProfile.deltaG) + ",";
        json += "\"db\":" + String((int)globalProfile.deltaB) + ",";
        json += "\"bri\":" + String((int)globalProfile.brightness) + ",";
        json += "\"con\":" + String((int)globalProfile.contrast) + ",";
        json += "\"sat\":" + String((int)globalProfile.saturation) + "},";
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
    if (!SD_MMC.exists(CONFIG_PATH)) return;

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
    for (uint8_t i = 0; i < DOMAIN_COUNT; ++i) domainProfiles[i] = ColorProfile();
    colorOverrides.clear();
    aplicarHardware();
}

// ================= BUCLE PRINCIPAL DEL TUNER =================

void runColorTuner(Arduino_GFX* gfx, Arduino_DataBus* bus, PNG* pngInstance) {
    _gfx = gfx;
    _bus = bus;
    _png = pngInstance;

    Serial.println("\n==================================================");
    Serial.println("[TUNER] TAMA COLOR TUNER EN EJECUCION");
    Serial.println("==================================================");

    loadColorConfigSD(_bus);
    redibujarTodo();

    bool running = true;
    while (running) {
        if (Serial.available()) {
            String line = Serial.readStringUntil('\n');
            line.trim();
            String upper = line;
            upper.toUpperCase();

            if (upper == "EXIT" || upper == "REBOOT") {
                Serial.println("[TUNER] Saliendo del calibrador...");
                if (bgCache) { free(bgCache); bgCache = nullptr; }
                delay(300);
                ESP.restart();
                return;
            }
            else if (upper == "SAVE") {
                saveColorConfigSD();
                redibujarHud();
            }
            else if (upper == "FACTORY" || upper == "FACTORY_RESET") {
                factoryResetColorSD(_bus);
                redibujarTodo();
            }
            else if (upper == "GET_PROFILES") {
                Serial.println("!PROFILES:" + serializeProfilesJson());
            }
            else if (upper == "GET_RAW_CFG") {
                Serial.println("!RAW_CFG_START");
                Serial.print(getRawConfigString());
                Serial.println("!RAW_CFG_END");
            }
            // PROTOCOLO MULTILÍNEA DE RECEPCIÓN DIRECTA
            else if (upper == "SET_RAW_START" || upper == "!SET_RAW_START") {
                String rawBuffer = "";
                while (true) {
                    String sub = Serial.readStringUntil('\n');
                    sub.trim();
                    if (sub == "SET_RAW_END" || sub == "!SET_RAW_END") break;
                    if (sub.length() > 0) rawBuffer += sub + "\n";
                }
                setRawConfigString(rawBuffer);
                redibujarTodo();
                Serial.println("[TUNER] Nueva configuracion multiline guardada y aplicada.");
            }
            else if (upper == "LIGHTS ON") {
                previewLightsOn = true;
                redibujarTodo();
            }
            else if (upper == "LIGHTS OFF") {
                previewLightsOn = false;
                redibujarTodo();
            }
            else if (upper.startsWith("NIGHT_DIM ")) {
                int val = line.substring(10).toInt();
                nightDimPercent = constrain(val, 0, 100);
                redibujarTodo();
            }
            else if (upper.startsWith("GLOBAL_TRIM ")) {
                int dr = 0, dg = 0, db = 0, bri = 0, con = 0, sat = 0;
                int count = sscanf(line.c_str() + 12, "%d %d %d %d %d %d", &dr, &dg, &db, &bri, &con, &sat);
                if (count == 6) {
                    globalProfile.deltaR = constrain(dr, -100, 100);
                    globalProfile.deltaG = constrain(dg, -100, 100);
                    globalProfile.deltaB = constrain(db, -100, 100);
                    globalProfile.brightness = constrain(bri, -100, 100);
                    globalProfile.contrast = constrain(con, -100, 100);
                    globalProfile.saturation = constrain(sat, -100, 100);
                    redibujarTodo();
                }
            }
            else if (upper.startsWith("DOMAIN ")) {
                String dName = line.substring(7);
                dName.trim();
                ColorDomain prevDomain = activeTunerDomain;
                activeTunerDomain = parseColorDomain(dName);
                if (activeTunerDomain == DOMAIN_WORLD || prevDomain == DOMAIN_WORLD) {
                    redibujarTodo();
                } else {
                    redibujarHud();
                }
            }
            else if (upper.startsWith("DOM_SWAPRB ")) {
                int val = line.substring(11).toInt();
                domainProfiles[activeTunerDomain].swapRB = (val == 1);
                if (activeTunerDomain == DOMAIN_TAMA) redibujarTama(); else redibujarTodo();
            }
            else if (upper.startsWith("DOM_INV ")) {
                int val = line.substring(8).toInt();
                domainProfiles[activeTunerDomain].invert = (val == 1);
                if (activeTunerDomain == DOMAIN_TAMA) redibujarTama(); else redibujarTodo();
            }
            else if (upper.startsWith("COLOR_RESET ")) {
                String hStr = line.substring(12);
                hStr.trim();
                uint16_t cKey = (uint16_t)strtoul(hStr.c_str(), NULL, 16);
                colorOverrides.erase(cKey);
                if (activeTunerDomain == DOMAIN_TAMA) redibujarTama(); else redibujarTodo();
            }
            else if (upper.startsWith("COLOR ")) {
                char hBuf[16] = {0};
                int dr = 0, dg = 0, db = 0, bri = 0, con = 0, sat = 0;
                int count = sscanf(line.c_str() + 6, "%s %d %d %d %d %d %d", hBuf, &dr, &dg, &db, &bri, &con, &sat);
                if (count == 7) {
                    uint16_t cKey = (uint16_t)strtoul(hBuf, NULL, 16);
                    ColorProfile& p = colorOverrides[cKey];
                    p.deltaR = constrain(dr, -100, 100);
                    p.deltaG = constrain(dg, -100, 100);
                    p.deltaB = constrain(db, -100, 100);
                    p.brightness = constrain(bri, -100, 100);
                    p.contrast = constrain(con, -100, 100);
                    p.saturation = constrain(sat, -100, 100);

                    if (activeTunerDomain == DOMAIN_TAMA) redibujarTama(); else redibujarTodo();
                }
            }
            else if (upper.startsWith("TRIM ")) {
                char dBuf[24] = {0};
                int dr = 0, dg = 0, db = 0, bri = 0, con = 0, sat = 0;
                int count = sscanf(line.c_str() + 5, "%s %d %d %d %d %d %d", dBuf, &dr, &dg, &db, &bri, &con, &sat);
                if (count == 7) {
                    ColorDomain targetDom = parseColorDomain(String(dBuf));
                    activeTunerDomain = targetDom;
                    ColorProfile& p = domainProfiles[targetDom];
                    p.deltaR = constrain(dr, -100, 100);
                    p.deltaG = constrain(dg, -100, 100);
                    p.deltaB = constrain(db, -100, 100);
                    p.brightness = constrain(bri, -100, 100);
                    p.contrast = constrain(con, -100, 100);
                    p.saturation = constrain(sat, -100, 100);

                    if (targetDom == DOMAIN_TAMA) redibujarTama(); else redibujarTodo();
                }
            }
            else if (upper.startsWith("BASE ")) {
                int r = 0, g = 0, b = 0;
                if (sscanf(line.c_str() + 5, "%d %d %d", &r, &g, &b) == 3) {
                    baseR = constrain(r, 0, 255);
                    baseG = constrain(g, 0, 255);
                    baseB = constrain(b, 0, 255);
                    if (activeTunerDomain == DOMAIN_TAMA) redibujarTama(); else redibujarTodo();
                }
            }
            else if (upper == "RGB") {
                currentBGR = false;
                aplicarHardware();
                redibujarTodo();
            }
            else if (upper == "BGR") {
                currentBGR = true;
                aplicarHardware();
                redibujarTodo();
            }
            else if (upper == "INV ON") {
                currentInv = true;
                aplicarHardware();
                redibujarTodo();
            }
            else if (upper == "INV OFF") {
                currentInv = false;
                aplicarHardware();
                redibujarTodo();
            }
            else if (upper == "SWAP") {
                swapPngBytes = !swapPngBytes;
                aplicarHardware();
                redibujarTodo();
            }
        }
        delay(10);
    }
}
