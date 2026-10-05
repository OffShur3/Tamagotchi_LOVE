// src/ColorTuner.cpp
#include "ColorTuner.h"
#include <SD_MMC.h>

namespace {
    static Arduino_GFX* _gfx = nullptr;
    static Arduino_DataBus* _bus = nullptr;
    static PNG* _png = nullptr;

    static uint16_t lineBuffer[320];
    static File pngFile;

    const char* CONFIG_PATH = "/config/color_tuning.txt";

    // Registros de Hardware
    static bool currentBGR = true;       // MADCTL: 0x48 (BGR) vs 0x40 (RGB)
    static bool currentInv = false;      // 0x21 (INVON) vs 0x20 (INVOFF)
    static bool swapPngBytes = false;

    // Tabla de Perfiles por Dominio
    static ColorProfile domainProfiles[DOMAIN_COUNT];

    // Dominio activo actualmente en el modo interactivo
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
}

const char* getColorDomainName(ColorDomain domain) {
    if (domain >= DOMAIN_COUNT) return "UNKNOWN";
    return DOMAIN_NAMES[domain];
}

ColorDomain parseColorDomain(const String& name) {
    for (uint8_t i = 0; i < DOMAIN_COUNT; ++i) {
        if (name.equalsIgnoreCase(DOMAIN_NAMES[i])) {
            return (ColorDomain)i;
        }
    }
    return DOMAIN_TAMA;
}

ColorProfile& getColorProfile(ColorDomain domain) {
    if (domain >= DOMAIN_COUNT) return domainProfiles[DOMAIN_TAMA];
    return domainProfiles[domain];
}

bool getHardwareBGR() { return currentBGR; }
bool getHardwareInv() { return currentInv; }
bool getHardwareSwapBytes() { return swapPngBytes; }

bool colorTunerHasEffect(ColorDomain domain) {
    if (domain >= DOMAIN_COUNT) return false;
    return (!domainProfiles[domain].isNeutral() || swapPngBytes);
}

uint16_t colorTunerApply(ColorDomain domain, uint8_t r, uint8_t g, uint8_t b) {
    if (domain >= DOMAIN_COUNT) domain = DOMAIN_WORLD;
    const ColorProfile& p = domainProfiles[domain];

    // Paso 1: Compensación de canal + Exposición (Ganancia Proporcional)
    float gain = max(0.0f, 1.0f + ((float)p.brightness / 100.0f));
    float r1 = clampU8(((float)r + p.deltaR) * gain);
    float g1 = clampU8(((float)g + p.deltaG) * gain);
    float b1 = clampU8(((float)b + p.deltaB) * gain);

    // Paso 2: Contraste centrado en gris medio (128)
    float cFactor = max(0.0f, 1.0f + ((float)p.contrast / 100.0f));
    float r2 = clampU8(128.0f + (r1 - 128.0f) * cFactor);
    float g2 = clampU8(128.0f + (g1 - 128.0f) * cFactor);
    float b2 = clampU8(128.0f + (b1 - 128.0f) * cFactor);

    // Paso 3: Saturación Perceptiva contra Luminancia Rec. 601 (Y)
    uint32_t Y = (77U * (uint32_t)r2 + 150U * (uint32_t)g2 + 29U * (uint32_t)b2) >> 8;
    float sFactor = max(0.0f, 1.0f + ((float)p.saturation / 100.0f));

    uint8_t rFinal = clampU8((float)Y + (r2 - (float)Y) * sFactor);
    uint8_t gFinal = clampU8((float)Y + (g2 - (float)Y) * sFactor);
    uint8_t bFinal = clampU8((float)Y + (b2 - (float)Y) * sFactor);

    uint16_t c565 = rgbTo565(rFinal, gFinal, bFinal);
    if (swapPngBytes) c565 = __builtin_bswap16(c565);
    return c565;
}

uint16_t colorTunerCorrect565(ColorDomain domain, uint16_t c565) {
    if (swapPngBytes) c565 = __builtin_bswap16(c565);
    uint8_t r = ((c565 >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((c565 >> 5)  & 0x3F) * 255 / 63;
    uint8_t b = (c565 & 0x1F)        * 255 / 31;
    return colorTunerApply(domain, r, g, b);
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

static void tunerPngClose(void *handle) { 
    if (pngFile) pngFile.close(); 
}

static int32_t tunerPngRead(PNGFILE *handle, uint8_t *buffer, int32_t length) { 
    return pngFile.read(buffer, length); 
}

static int32_t tunerPngSeek(PNGFILE *handle, int32_t position) { 
    return pngFile.seek(position) ? position : -1; 
}

static int tunerPngDraw(PNGDRAW *pDraw) {
    int y = pDraw->y + 215;
    if (y >= 320) return 1;

    if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
        uint8_t* src = (uint8_t*)pDraw->pPixels;
        for (int x = 0; x < pDraw->iWidth && x < 172; ++x) {
            lineBuffer[x] = colorTunerApply(activeTunerDomain, src[0], src[1], src[2]);
            src += 4;
        }
    } else {
        _png->getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0);
        for (int x = 0; x < pDraw->iWidth && x < 172; ++x) {
            lineBuffer[x] = colorTunerCorrect565(activeTunerDomain, lineBuffer[x]);
        }
    }
    _gfx->draw16bitRGBBitmap(10, y, lineBuffer, min(pDraw->iWidth, 152), 1);
    return 1;
}

static void redibujar() {
    if (!_gfx) return;
    _gfx->fillScreen(0x0000);

    // 1. Barras de Referencia RGB puras
    _gfx->fillRect(10, 8, 48, 20, 0xF800);
    _gfx->fillRect(62, 8, 48, 20, 0x07E0);
    _gfx->fillRect(114, 8, 48, 20, 0x001F);
    _gfx->setTextSize(1); _gfx->setTextColor(0xFFFF);
    _gfx->setCursor(18, 30); _gfx->print("ROJO");
    _gfx->setCursor(68, 30); _gfx->print("VERDE");
    _gfx->setCursor(124, 30); _gfx->print("AZUL");

    // 2. Comparador: Color Base vs Color Corregido
    uint16_t colOriginal = rgbTo565(baseR, baseG, baseB);
    uint16_t colAjustado = colorTunerApply(activeTunerDomain, baseR, baseG, baseB);

    _gfx->drawRect(10, 42, 73, 40, 0xFFFF);
    _gfx->fillRect(12, 44, 69, 36, colOriginal);
    _gfx->setCursor(14, 85); _gfx->print("ORIGINAL");

    _gfx->drawRect(89, 42, 73, 40, 0xFFFF);
    _gfx->fillRect(91, 44, 69, 36, colAjustado);
    _gfx->setCursor(95, 85); _gfx->print("CORREGIDO");

    // 3. Telemetría de la calibración del dominio activo
    const ColorProfile& cp = domainProfiles[activeTunerDomain];
    _gfx->setCursor(10, 98);
    _gfx->printf("DOMINIO: [%s]", DOMAIN_NAMES[activeTunerDomain]);
    _gfx->setCursor(10, 110);
    _gfx->printf("dR:%+d dG:%+d dB:%+d", cp.deltaR, cp.deltaG, cp.deltaB);
    _gfx->setCursor(10, 122);
    _gfx->printf("Bri:%+d Con:%+d Sat:%+d", cp.brightness, cp.contrast, cp.saturation);

    _gfx->setCursor(10, 138);
    _gfx->printf("SD Config: %s", SD_MMC.exists(CONFIG_PATH) ? "ACTIVA" : "DEFECTO");
    _gfx->setCursor(10, 150);
    _gfx->printf("565: 0x%04X -> 0x%04X", colOriginal, colAjustado);

    // 4. Muestra de Sprite según el dominio seleccionado
    const char* samplePath = "/tama/ui/icons_ui.png";
    if (activeTunerDomain == DOMAIN_TAMA && SD_MMC.exists("/tama/sprites/base/tiernito/bebe/idle.png")) {
        samplePath = "/tama/sprites/base/tiernito/bebe/idle.png";
    } else if (activeTunerDomain == DOMAIN_WORLD && SD_MMC.exists("/tama/ui/bg_main.png")) {
        samplePath = "/tama/ui/bg_main.png";
    }

    if (SD_MMC.exists(samplePath)) {
        int rc = _png->open(samplePath, tunerPngOpen, tunerPngClose, tunerPngRead, tunerPngSeek, tunerPngDraw);
        if (rc == PNG_SUCCESS) {
            _png->decode(NULL, 0);
            _png->close();
        }
    }
}

// ================= SERIALIZACIÓN / PERSISTENCIA EN SD =================

String serializeProfilesJson() {
    String json = "{";
    json += "\"bgr\":" + String(currentBGR ? 1 : 0) + ",";
    json += "\"inv\":" + String(currentInv ? 1 : 0) + ",";
    json += "\"swap\":" + String(swapPngBytes ? 1 : 0) + ",";
    json += "\"domains\":{";
    for (uint8_t i = 0; i < DOMAIN_COUNT; ++i) {
        const ColorProfile& p = domainProfiles[i];
        json += "\"" + String(DOMAIN_NAMES[i]) + "\":{";
        json += "\"dr\":" + String((int)p.deltaR) + ",";
        json += "\"dg\":" + String((int)p.deltaG) + ",";
        json += "\"db\":" + String((int)p.deltaB) + ",";
        json += "\"bri\":" + String((int)p.brightness) + ",";
        json += "\"con\":" + String((int)p.contrast) + ",";
        json += "\"sat\":" + String((int)p.saturation) + "}";
        if (i < DOMAIN_COUNT - 1) json += ",";
    }
    json += "}}";
    return json;
}

void loadColorConfigSD(Arduino_DataBus* bus) {
    if (bus) _bus = bus;
    if (!SD_MMC.exists(CONFIG_PATH)) {
        Serial.println("[COLOR-SD] Sin tuning previo en SD. Inicializando perfiles neutros.");
        return;
    }

    File f = SD_MMC.open(CONFIG_PATH, "r");
    if (!f) return;

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
                }
                start = semi + 1;
            }
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
                }
                start = semi + 1;
            }
        }
    }
    f.close();

    Serial.println("[COLOR-SD] Perfiles multidominio cargados exitosamente de la SD.");
    aplicarHardware();
}

void saveColorConfigSD() {
    SD_MMC.mkdir("/config");
    File f = SD_MMC.open(CONFIG_PATH, "w");
    if (!f) {
        Serial.println("[COLOR-SD] Error: No se pudo escribir /config/color_tuning.txt");
        return;
    }

    f.printf("global:bgr=%d;inv=%d;swap=%d\n", currentBGR ? 1 : 0, currentInv ? 1 : 0, swapPngBytes ? 1 : 0);
    for (uint8_t i = 0; i < DOMAIN_COUNT; ++i) {
        const ColorProfile& p = domainProfiles[i];
        f.printf("%s:dr=%d;dg=%d;db=%d;bri=%d;con=%d;sat=%d\n",
                 DOMAIN_NAMES[i], (int)p.deltaR, (int)p.deltaG, (int)p.deltaB,
                 (int)p.brightness, (int)p.contrast, (int)p.saturation);
    }
    f.close();
    Serial.println("[COLOR-SD] Configuración multidominio guardada permanentemente en SD.");
}

void factoryResetColorSD(Arduino_DataBus* bus) {
    if (bus) _bus = bus;
    if (SD_MMC.exists(CONFIG_PATH)) {
        SD_MMC.remove(CONFIG_PATH);
        Serial.println("[COLOR-SD] Archivo de calibración eliminado de la SD.");
    }

    currentBGR = true;
    currentInv = false;
    swapPngBytes = false;
    for (uint8_t i = 0; i < DOMAIN_COUNT; ++i) {
        domainProfiles[i] = ColorProfile();
    }

    aplicarHardware();
    Serial.println("[COLOR-SD] Valores de fábrica restaurados para todos los dominios.");
}

// ================= BUCLE PRINCIPAL DEL TUNER =================

void runColorTuner(Arduino_GFX* gfx, Arduino_DataBus* bus, PNG* pngInstance) {
    _gfx = gfx;
    _bus = bus;
    _png = pngInstance;

    loadColorConfigSD(_bus);

    Serial.println("\n=======================================================");
    Serial.println("  MODO CALIBRADOR EN EJECUCIÓN (TAMA COLOR TUNER)");
    Serial.println("=======================================================");
    Serial.println("  Protocolo Serial multidominio disponible:");
    Serial.println("  TRIM <DOMAIN> <dr> <dg> <db> <bri> <con> <sat>");
    Serial.println("  DOMAIN <DOMAIN_NAME>");
    Serial.println("  GET_PROFILES");
    Serial.println("  SAVE | FACTORY | EXIT");
    Serial.println("=======================================================\n");

    redibujar();

    bool running = true;
    while (running) {
        if (Serial.available()) {
            String line = Serial.readStringUntil('\n');
            line.trim();
            String upper = line;
            upper.toUpperCase();

            if (upper == "EXIT" || upper == "REBOOT") {
                Serial.println("[TUNER] Saliendo del calibrador. Reiniciando hacia TAMA...");
                delay(300);
                ESP.restart();
                return;
            }
            else if (upper == "SAVE") {
                saveColorConfigSD();
                redibujar();
            }
            else if (upper == "FACTORY" || upper == "FACTORY_RESET") {
                factoryResetColorSD(_bus);
                redibujar();
            }
            else if (upper == "GET_PROFILES") {
                Serial.println("!PROFILES:" + serializeProfilesJson());
            }
            else if (upper.startsWith("DOMAIN ")) {
                String dName = line.substring(7);
                dName.trim();
                activeTunerDomain = parseColorDomain(dName);
                redibujar();
                Serial.printf("[TUNER] Dominio activo: %s\n", DOMAIN_NAMES[activeTunerDomain]);
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
                    redibujar();
                }
            }
            else if (upper.startsWith("BASE ")) {
                int r = 0, g = 0, b = 0;
                if (sscanf(line.c_str() + 5, "%d %d %d", &r, &g, &b) == 3) {
                    baseR = constrain(r, 0, 255);
                    baseG = constrain(g, 0, 255);
                    baseB = constrain(b, 0, 255);
                    redibujar();
                }
            }
            else if (upper == "RESET") {
                domainProfiles[activeTunerDomain] = ColorProfile();
                redibujar();
            }
            else if (upper == "RGB") {
                currentBGR = false;
                aplicarHardware();
                redibujar();
            }
            else if (upper == "BGR") {
                currentBGR = true;
                aplicarHardware();
                redibujar();
            }
            else if (upper == "INV ON") {
                currentInv = true;
                aplicarHardware();
                redibujar();
            }
            else if (upper == "INV OFF") {
                currentInv = false;
                aplicarHardware();
                redibujar();
            }
            else if (upper == "SWAP") {
                swapPngBytes = !swapPngBytes;
                aplicarHardware();
                redibujar();
            }
        }
        delay(15);
    }
}
