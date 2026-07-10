#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

// Librerias
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Arduino_GFX_Library.h>
#include <PNGdec.h>

// Archivos
#include "UI.h"

extern WebServer server;
extern DNSServer dnsServer;
extern Preferences preferences;

extern Arduino_GFX *gfx;
extern PNG png;
extern void *pngOpen(const char *filename, int32_t *size);
extern void pngClose(void *handle);
extern int32_t pngRead(PNGFILE *handle, uint8_t *buffer, int32_t length);
extern int32_t pngSeek(PNGFILE *handle, int32_t position);
extern int pngDraw(PNGDRAW *pDraw);
extern int pngOffsetX;
extern int pngOffsetY;
extern bool redConfigurada;
extern bool leerTouch(uint16_t &x, uint16_t &y); 

void inicializarRed();
bool pantallaOpcionRed();
bool iniciarPortalCautivo();
void procesarPortal();
void cargarPaginaConfig();
void guardarCredenciales();
void borrarRedes();
void accionConectarRed();
void accionOffline();

#endif
