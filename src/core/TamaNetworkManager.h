// src/core/TamaNetworkManager.h
#pragma once
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Arduino_GFX_Library.h>
#include <PNGdec.h>

class TamaNetworkManager {
public:
    struct Config {
        Arduino_GFX* gfx;
        PNG* png;
        const char* qrPath;             // "/QR Network.png"
        const char* apSSID;             // "TAMA-WiFi-Config"
        const char* apPassword;         // Clave AP
        void* (*pngOpen)(const char*, int32_t*);
        void (*pngClose)(void*);
        int32_t (*pngRead)(PNGFILE*, uint8_t*, int32_t);
        int32_t (*pngSeek)(PNGFILE*, int32_t);
        int (*pngDraw)(PNGDRAW*);
        bool (*checkExit)();            // Callback inyectado para verificar Swipe Left de salida
    };

    TamaNetworkManager(const Config& config);
    ~TamaNetworkManager();

    void begin();
    bool isConnected();
    void connectSavedNetworks();
    
    bool runCaptivePortal();
    void clearSavedNetworks();

private:
    Config _cfg;
    WebServer _server;
    DNSServer _dnsServer;
    WiFiMulti _wifiMulti;
    bool _hasConfiguredNetwork;

    void setupAP();
    void handleRoot();
    void handleSave();
    void handleNotFound();
    bool drawQRCode();
};
