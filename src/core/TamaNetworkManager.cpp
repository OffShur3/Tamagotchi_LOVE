// src/core/TamaNetworkManager.cpp
#include "TamaNetworkManager.h"
#include <vector>
#include <algorithm> // Requerido para std::swap
#include <ArduinoJson.h> 
#include <SD_MMC.h>

TamaNetworkManager::TamaNetworkManager(const Config& config) 
    : _cfg(config), _server(80), _hasConfiguredNetwork(false) {}

TamaNetworkManager::~TamaNetworkManager() {
    _server.stop();
    _dnsServer.stop();
}

bool TamaNetworkManager::begin() {
    if (!_cfg.jsonPath || !SD_MMC.exists(_cfg.jsonPath)) {
        Serial.println("[PORTAL] No se detecto archivo de red wifi.json en la SD.");
        return false;
    }

    File file = SD_MMC.open(_cfg.jsonPath, "r");
    if (!file) return false;

    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.println("[PORTAL] Error de decodificacion en wifi.json.");
        return false;
    }

    JsonArray arr = doc.as<JsonArray>();
    int redesRegistradas = 0;
    
    for (JsonObject obj : arr) {
        const char* ssid = obj["ssid"];
        const char* pass = obj["pass"];
        if (ssid && strlen(ssid) > 0) {
            _wifiMulti.addAP(ssid, pass);
            Serial.printf("[PORTAL] Red registrada en Multi-WiFi: %s\n", ssid);
            redesRegistradas++;
        }
    }
    
    _hasConfiguredNetwork = (redesRegistradas > 0);
    return _hasConfiguredNetwork;
}

bool TamaNetworkManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void TamaNetworkManager::runBackgroundConnect() {
    WiFi.mode(WIFI_STA);
    if (_hasConfiguredNetwork) {
        _wifiMulti.run();
    }
}

bool TamaNetworkManager::runCaptivePortal() {
    setupAP();
    _dnsServer.start(53, "*", WiFi.softAPIP());

    _server.on("/", [this]() { handleRoot(); });
    _server.on("/save", [this]() { handleSave(); }); 
    _server.onNotFound([this]() { handleNotFound(); });
    _server.begin();

    Serial.println("[PORTAL] Servidor y DNS corriendo. Esperando credenciales...");

    bool exitPortal = false;
    while (!exitPortal) {
        _dnsServer.processNextRequest();
        _server.handleClient();
        delay(10);
        
        if (_cfg.checkExit && _cfg.checkExit()) {
            Serial.println("[PORTAL] Swipe de salida detectado.");
            exitPortal = true;
        }

        // Telemetría Serial interactiva durante el portal cautivo
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();
            cmd.toUpperCase();
            if (cmd == "INFO" || cmd == "HEAP" || cmd == "RAM") {
                Serial.println("\n==================================================");
                Serial.println("     TAMA KERNEL PORTAL DIAGNOSTICS (ALIVE)       ");
                Serial.println("==================================================");
                Serial.printf("  Free Heap RAM:      %7u Bytes (%u KB)\n", ESP.getFreeHeap(), ESP.getFreeHeap() / 1024);
                Serial.printf("  Min Free Heap:      %7u Bytes (%u KB)\n", ESP.getMinFreeHeap(), ESP.getMinFreeHeap() / 1024);
                Serial.printf("  Bloque contiguo Max:%7u Bytes (%u KB)\n", 
                              heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
                Serial.println("==================================================");
            }
        }
    }

    _server.stop();
    _dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    return !exitPortal; 
}

bool TamaNetworkManager::drawQRCode() {
    if (!_cfg.png || !_cfg.qrPath) return false;
    
    int rc = _cfg.png->open(_cfg.qrPath, _cfg.pngOpen, _cfg.pngClose, _cfg.pngRead, _cfg.pngSeek, _cfg.pngDraw);
    if (rc == PNG_SUCCESS) {
        _cfg.png->decode(NULL, 0);
        _cfg.png->close();
        return true;
    }
    return false;
}

void TamaNetworkManager::setupAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(_cfg.apSSID, _cfg.apPassword);
    delay(100);
}

void TamaNetworkManager::drawPortalScreen() {
    _cfg.gfx->fillScreen(0x1042); // MAT_BG

    if (!drawQRCode()) {
        _cfg.gfx->setTextColor(0xF800); // Rojo
        _cfg.gfx->setTextSize(1);
        _cfg.gfx->setCursor(30, 100);
        _cfg.gfx->print("QR no encontrado");
    }

    _cfg.gfx->setTextSize(1);
    _cfg.gfx->setTextColor(0xFEE0); // Amarillo
    _cfg.gfx->setCursor((172 - (21 * 6)) / 2, 135);
    _cfg.gfx->print("Conecta tu celular a:");

    _cfg.gfx->setTextSize(2);
    _cfg.gfx->setTextColor(0xFFFF); // Blanco
    _cfg.gfx->setCursor((172 - (10 * 12)) / 2, 150);
    _cfg.gfx->print(_cfg.apSSID);
    
    _cfg.gfx->setTextSize(1);
    _cfg.gfx->setTextColor(0x07E0); // Verde
    _cfg.gfx->setCursor((172 - (15 * 6)) / 2, 185);
    _cfg.gfx->print("Admite conexion");
    _cfg.gfx->setCursor((172 - (12 * 6)) / 2, 200);
    _cfg.gfx->print("sin Internet");
    
    _cfg.gfx->setTextColor(0xFFFF); // Blanco
    _cfg.gfx->setCursor((172 - (18 * 6)) / 2, 230);
    _cfg.gfx->print("Entra al navegador");
    _cfg.gfx->setCursor((172 - (15 * 6)) / 2, 245);
    _cfg.gfx->print("y elige tu WiFi");
    
    _cfg.gfx->setTextSize(2);
    _cfg.gfx->setCursor((172 - (2 * 12)) / 2, 270);
    _cfg.gfx->print("<3");
}

void TamaNetworkManager::handleRoot() {
    int n = WiFi.scanNetworks();
    
    std::vector<int> indices(n);
    for (int i = 0; i < n; i++) indices[i] = i;

    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
                std::swap(indices[i], indices[j]);
            }
        }
    }

    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>TAMA WiFi Config</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background-color: #160816; color: white; text-align: center; padding: 20px; }";
    html += "select, input { width: 85%; padding: 12px; margin: 10px 0; border-radius: 6px; border: 2px solid #4c2818; background: #221022; color: white; font-size: 14px; }";
    html += "input[type=submit] { background-color: #43a047; color: white; font-weight: bold; cursor: pointer; border: none; }";
    html += "h1 { color: #fbeeaa; margin-bottom: 5px; }";
    html += "p { color: #f5dfbf; margin-top: 0; }";
    html += "</style></head><body>";
    html += "<h1>TAMA KERNEL</h1>";
    html += "<p>Configuracion de Red</p>";
    html += "<form action='/save' method='POST'>";
    html += "SSID / Red WiFi:<br>";
    html += "<select name='ssid'>";
    
    if (n == 0) {
        html += "<option value=''>No se encontraron redes</option>";
    } else {
        for (int i = 0; i < n; i++) {
            int idx = indices[i];
            String ssid = WiFi.SSID(idx);
            int32_t rssi = WiFi.RSSI(idx);
            String selected = (i == 0) ? " selected" : "";
            html += "<option value='" + ssid + "'" + selected + ">" + ssid + " (" + String(rssi) + " dBm)</option>";
        }
    }
    
    html += "</select><br>";
    html += "Clave:<br>";
    html += "<input type='password' name='pass' placeholder='Introduce la clave'><br>";
    html += "<input type='submit' value='Conectar TAMA'>";
    html += "</form></body></html>";

    _server.send(200, "text/html; charset=utf-8", html);
}

void TamaNetworkManager::handleSave() {
    String reqSSID = _server.arg("ssid");
    String reqPASS = _server.arg("pass");
    
    reqSSID.trim();
    reqPASS.trim();

    if (reqSSID.length() > 0) {
        StaticJsonDocument<2048> doc;
        
        File readFile = SD_MMC.open(_cfg.jsonPath, "r");
        if (readFile) {
            deserializeJson(doc, readFile);
            readFile.close();
        }

        JsonArray arr = doc.as<JsonArray>();
        if (arr.isNull()) {
            arr = doc.to<JsonArray>();
        }

        bool redExiste = false;
        for (JsonObject obj : arr) {
            const char* ssid = obj["ssid"];
            if (ssid && String(ssid) == reqSSID) {
                obj["pass"] = reqPASS;
                redExiste = true;
                Serial.println("[PORTAL] Red existente encontrada en SD. Contraseña actualizada.");
                break;
            }
        }

        if (!redExiste) {
            JsonObject newNet = arr.createNestedObject();
            newNet["ssid"] = reqSSID;
            newNet["pass"] = reqPASS;
            Serial.println("[PORTAL] Nueva red agregada a la lista de la SD.");
        }

        SD_MMC.mkdir("/tama/config");

        File writeFile = SD_MMC.open(_cfg.jsonPath, "w");
        if (writeFile) {
            serializeJson(doc, writeFile);
            writeFile.close();
            Serial.println("[PORTAL] Archivo wifi.json actualizado de forma persistente.");
        } else {
            Serial.println("[PORTAL] ERROR CRÍTICO: No se pudo escribir wifi.json en la SD.");
        }

        _server.send(200, "text/html", "<html>Configuracion guardada con exito. Reiniciando...</html>");
        delay(2000);
        ESP.restart();
    }
}

void TamaNetworkManager::handleNotFound() {
    _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    _server.send(302, "text/plain", "");
}
