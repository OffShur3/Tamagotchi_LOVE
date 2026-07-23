// src/core/TamaNetworkManager.h
#pragma once
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Arduino_GFX_Library.h>
#include <PNGdec.h>

class TamaNetworkManager {
public:
    struct Config {
        Arduino_GFX* gfx;
        PNG* png;
        const char* qrPath;             // "/QR Network.png"
        const char* jsonPath;           // "/config/wifi.json" (Sincronizado)
        const char* apSSID;             // "TamaConfig"
        const char* apPassword;         // Clave AP
        void* (*pngOpen)(const char*, int32_t*);
        void (*pngClose)(void*);
        int32_t (*pngRead)(PNGFILE*, uint8_t*, int32_t);
        int32_t (*pngSeek)(PNGFILE*, int32_t);
        int (*pngDraw)(PNGDRAW*);
        bool (*checkExit)();            // Callback de Swipe Right
    };

    TamaNetworkManager(const Config& config);
    ~TamaNetworkManager();

    // Carga de redes del archivo JSON de la SD
    bool begin(); 
    void runBackgroundConnect();
    bool isConnected();
    
    // Control del Portal Cautivo
    bool runCaptivePortal();
    void drawPortalScreen();
    void handleSave();

private:
    Config _cfg;
    WebServer _server;
    DNSServer _dnsServer;
    WiFiMulti _wifiMulti;
    bool _hasConfiguredNetwork;

    void setupAP();
    void handleRoot();
    void handleNotFound();
    bool drawQRCode();
};
