#include "NetworkManager.h"
#include "colors.h"

WebServer server(80);
DNSServer dnsServer;
Preferences preferences;
WiFiMulti wifiMulti;

const byte DNS_PORT = 53;

const char HTML_HEADER[] PROGMEM = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>TamaConfig - Wi-Fi</title><style>"
"body{font-family:Arial,sans-serif;background-color:#f4f7f6;color:#333;margin:0;padding:20px;text-align:center;}"
".card{background:white;padding:25px;border-radius:15px;box-shadow:0 4px 15px rgba(0,0,0,0.1);max-width:400px;margin:auto;}"
"h2{color:#4A90E2;margin-bottom:10px;}"
"p{font-size:14px;color:#666;}"
"select,input[type='password'],input[type='submit']{width:100%;padding:12px;margin:10px 0;border-radius:8px;border:1px solid #ccc;box-sizing:border-box;font-size:16px;}"
"select{background-color:#fff;}"
"input[type='submit']{background-color:#4A90E2;color:white;border:none;cursor:pointer;font-weight:bold;transition:0.3s;}"
"input[type='submit']:hover{background-color:#357ABD;}"
".footer{margin-top:20px;font-size:11px;color:#aaa;}"
"</style></head><body><div class='card'>";

bool redConfigurada = false; // Definición inicial

void inicializarRed() {
    if (redConfigurada) return;

    preferences.begin("wifi-config", false);
    int netCount = preferences.getInt("netCount", 0);

    if (netCount > 0) {
        for (int i = 0; i < netCount; i++) {
            String ssidKey = "ssid" + String(i);
            String passKey = "pass" + String(i);
            String s = preferences.getString(ssidKey.c_str(), "");
            String p = preferences.getString(passKey.c_str(), "");
            if (s != "") {
                wifiMulti.addAP(s.c_str(), p.c_str());
            }
        }
        preferences.end();

        // Intentar conectar sin bloquear la pantalla
        WiFi.mode(WIFI_STA);
        wifiMulti.run(); // Intenta conectar una vez, no se queda trabado
        if (WiFi.status() == WL_CONNECTED) {
            redConfigurada = true;
            Serial.println("[WiFi] Conectado correctamente en segundo plano");
        }
    } else {
        preferences.end();
    }
}

bool pantallaOpcionRed() {
    gfx->fillScreen(MAT_BG);
    
    bool resultado = mostrarPopup(
        "Sin Conexion",
        "No se pudo conectar\na ninguna red WiFi.",
        "Conectar",
        "Offline",
        accionConectarRed,
        accionOffline
    );
    
    // resultado es true si presionó "Conectar", false si presionó "Offline"
    gfx->fillScreen(MAT_BG);
    return resultado;
}

// Acción para "Conectar" - abre el portal cautivo
void accionConectarRed() {
    iniciarPortalCautivo();
}

// Acción para "Offline" - simplemente sale
void accionOffline() {
    // No hace nada, solo cierra el popup
}

bool iniciarPortalCautivo() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("TamaConfig", "iluvUiluvU<3");
    delay(100); 

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    server.on("/", cargarPaginaConfig);
    server.on("/save", HTTP_POST, guardarCredenciales);
    
    server.onNotFound([]() {
        server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
        server.send(302, "text/plain", "");
    });

    server.begin();

    gfx->fillScreen(MAT_BG);
    
    pngOffsetX = 36; pngOffsetY = 25;
    int rc = png.open("/QR Network.png", pngOpen, pngClose, pngRead, pngSeek, pngDraw);
    if (rc == PNG_SUCCESS) { png.decode(NULL, 0); png.close(); }
    pngOffsetX = 0; pngOffsetY = 0;

    gfx->setTextColor(BGR_YELLOW);
    imprimirCentrado("Conecta tu celular a:", 135, 1);
    
    gfx->setTextColor(BGR_WHITE);
    imprimirCentrado("TamaConfig", 150, 2);
    
    gfx->setTextColor(BGR_GREEN);
    imprimirCentrado("Admite conexion", 185, 1);
    imprimirCentrado("sin Internet", 200, 1);
    
    gfx->setTextColor(BGR_WHITE);
    imprimirCentrado("Entra al navegador", 230, 1);
    imprimirCentrado("y elige tu WiFi", 245, 1);
    
    imprimirCentrado("<3", 270, 2);

    // Bucle de monitoreo para gestos de navegación
    uint16_t startX = 0, startY = 0;
    bool gestando = false;
    uint16_t lastX = 0;

    while (true) {
        procesarPortal();

        uint16_t x, y;
        bool tocado = leerTouch(x, y);

        if (tocado) {
            if (!gestando) {
                gestando = true;
                startX = x;
            }
            lastX = x;
        } else if (gestando) {
            gestando = false;
            if ((lastX - startX) > 50) {
                server.stop();
                dnsServer.stop();
                WiFi.mode(WIFI_STA);
                return false;   // Cancelado por el usuario
            }
        }
        delay(10);
    }
}

void procesarPortal() {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(10);
}

void cargarPaginaConfig() {
    int n = WiFi.scanNetworks();
    
    String html = FPSTR(HTML_HEADER); 
    html += "<h2>TamaConfig</h2>";
    html += "<p>Selecciona la red de tu hogar para conectar tu Tamagotchi:</p>";
    html += "<form method='POST' action='/save'>";
    html += "<select name='ssid'>";
    
    if (n == 0) {
        html += "<option value=''>No se encontraron redes</option>";
    } else {
        for (int i = 0; i < n; ++i) {
            html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
        }
    }
    html += "</select>";
    html += "<input type='password' name='pass' placeholder='Contraseña de tu Wi-Fi' required>";
    html += "<input type='submit' value='Agregar Red y Conectar'>";
    html += "</form></div><div class='footer'>OpenSource Tamagotchi Project</div></body></html>";
    
    server.send(200, "text/html; charset=utf-8", html);
}

void guardarCredenciales() {
    String reqSSID = server.arg("ssid");
    String reqPASS = server.arg("pass");

    if (reqSSID != "") {
        preferences.begin("wifi-config", false);
        int netCount = preferences.getInt("netCount", 0);
        bool redExiste = false;

        for (int i = 0; i < netCount; i++) {
            String savedSSID = preferences.getString(("ssid" + String(i)).c_str(), "");
            if (savedSSID == reqSSID) {
                preferences.putString(("pass" + String(i)).c_str(), reqPASS);
                redExiste = true;
                break;
            }
        }

        if (!redExiste) {
            preferences.putString(("ssid" + String(netCount)).c_str(), reqSSID);
            preferences.putString(("pass" + String(netCount)).c_str(), reqPASS);
            preferences.putInt("netCount", netCount + 1);
        }
        
        preferences.end();

        String html = FPSTR(HTML_HEADER); 
        html += "<h2>¡Datos Recibidos!</h2>";
        html += "<p>Tu Tamagotchi ha guardado la red <b>" + reqSSID + "</b> y se esta reiniciando.</p>";
        html += "<p>Ya puedes cerrar esta ventana.</p>";
        html += "</div></body></html>";
        
        server.send(200, "text/html", html);
        delay(2000);
        
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Error: SSID Invalido");
    }
}

void borrarRedes() {
    preferences.begin("wifi-config", false);
    preferences.clear();
    preferences.end();
    Serial.println("\n[!] Todas las redes Wi-Fi han sido borradas de la memoria.");
}
