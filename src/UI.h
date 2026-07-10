#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "colors.h"

// Referencia al objeto global de la pantalla
extern Arduino_GFX *gfx;
extern bool leerTouch(uint16_t &x, uint16_t &y); 

// Prototipos de funciones de interfaz
void imprimirCentrado(String texto, int y, int size);
void dibujarBoton(int x, int y, int w, int h, int r, uint16_t colorFondo, String texto, int textSize, uint16_t colorTexto);
void esperarSoltar();

// Popup genérico con dos botones (uno arriba del otro)
// Retorna true si presionó el botón superior, false si presionó el inferior
bool mostrarPopup(String header, String body, String btnSi, String btnNo,
                  void (*accionSi)(), void (*accionNo)());


#endif
