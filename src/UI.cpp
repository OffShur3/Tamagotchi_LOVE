#include "UI.h"

void imprimirCentrado(String texto, int y, int size) {
    gfx->setTextSize(size);
    int anchoPorChar = size * 6;
    int anchoTexto = texto.length() * anchoPorChar;
    int x = (gfx->width() - anchoTexto) / 2;
    
    gfx->setCursor(x, y);
    gfx->println(texto);
}

void dibujarBoton(int x, int y, int w, int h, int r, uint16_t colorFondo, String texto, int textSize, uint16_t colorTexto) {
    // 1. Dibujar el cuerpo del botón
    gfx->fillRoundRect(x, y, w, h, r, colorFondo);
    
    // 2. Calcular el tamaño del texto para centrarlo perfectamente dentro del rectángulo
    gfx->setTextSize(textSize);
    int anchoPorChar = textSize * 6;
    int altoPorChar = textSize * 8; // La fuente por defecto mide aprox 8px de alto base
    
    int anchoTexto = texto.length() * anchoPorChar;
    
    // 3. Ecuación para centrar: (posición del contenedor) + (espacio sobrante / 2)
    int textX = x + (w - anchoTexto) / 2;
    int textY = y + (h - altoPorChar) / 2;
    
    // 4. Imprimir el texto
    gfx->setTextColor(colorTexto);
    gfx->setCursor(textX, textY);
    gfx->print(texto);
}
