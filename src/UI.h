#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "colors.h"

// Referencia al objeto global de la pantalla
extern Arduino_GFX *gfx;

// Prototipos de funciones de interfaz
void imprimirCentrado(String texto, int y, int size);
void dibujarBoton(int x, int y, int w, int h, int r, uint16_t colorFondo, String texto, int textSize, uint16_t colorTexto);

#endif
