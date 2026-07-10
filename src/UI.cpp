#include "UI.h"

void imprimirCentrado(String texto, int y, int size) {
    gfx->setTextSize(size);
    int anchoPorChar = size * 6;
    int anchoTexto = texto.length() * anchoPorChar;
    int x = (gfx->width() - anchoTexto) / 2;
    
    gfx->setCursor(x, y);
    gfx->println(texto);
}

// --------------- Esperar a que el usuario suelte el dedo ---------------
void esperarSoltar() {
    uint16_t tx, ty;
    while (leerTouch(tx, ty)) {
        delay(10);
    }
    delay(100); // Pequeño margen de seguridad
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

bool mostrarPopup(String header, String body, String btnSi, String btnNo,
                  void (*accionSi)(), void (*accionNo)()) {
    // Dibujar fondo del popup
    gfx->fillRoundRect(10, 70, 152, 190, 12, MAT_BG);
    gfx->drawRoundRect(10, 70, 152, 190, 12, BGR_WHITE);

    // Header (título)
    gfx->setTextColor(BGR_WHITE);
    gfx->setTextSize(1);
    imprimirCentrado(header, 85, 1);

    // Body (puede ser multilínea simple con \n)
    gfx->setTextColor(BGR_YELLOW);
    gfx->setTextSize(1);
    int yPos = 110;
    String linea = "";
    for (int i = 0; i <= body.length(); i++) {
        if (body[i] == '\n' || body[i] == '\0') {
            imprimirCentrado(linea, yPos, 1);
            yPos += 15;
            linea = "";
        } else {
            linea += body[i];
        }
    }

    // Botones (uno arriba del otro)
    dibujarBoton(20, 165, 132, 35, 8, MAT_CONNECT, btnSi, 1, BGR_WHITE);
    dibujarBoton(20, 210, 132, 35, 8, MAT_OFFLINE, btnNo, 1, BGR_WHITE);

    esperarSoltar();

    uint16_t tx, ty;
    while (true) {
        if (leerTouch(tx, ty)) {
            // Botón superior (Si)
            if (tx >= 20 && tx <= 152 && ty >= 165 && ty <= 200) {
                esperarSoltar();
                if (accionSi) accionSi();
                return true;
            }
            // Botón inferior (No)
            if (tx >= 20 && tx <= 152 && ty >= 210 && ty <= 245) {
                esperarSoltar();
                if (accionNo) accionNo();
                return false;
            }
        }
        delay(50);
    }
}
