#include "PhotoCarousel.h"
#include "colors.h"

PhotoCarousel::PhotoCarousel(Arduino_GFX* gfx, PNG* png,
                             void* (*openFn)(const char*, int32_t*),
                             void (*closeFn)(void*),
                             int32_t (*readFn)(PNGFILE*, uint8_t*, int32_t),
                             int32_t (*seekFn)(PNGFILE*, int32_t),
                             int (*drawFn)(PNGDRAW*))
    : gfx(gfx), png(png), pngOpen(openFn), pngClose(closeFn),
      pngRead(readFn), pngSeek(seekFn), pngDraw(drawFn) {}

void PhotoCarousel::loadImages() {
    File root = SD_MMC.open("/");
    File file = root.openNextFile();
    while (file && imageCount < MAX_IMAGES) {
        if (!file.isDirectory()) {
            String name = file.name();
            name.toLowerCase();
            if (name.endsWith(".png") && name != "qr network.png") {
                imageFiles[imageCount++] = file.name();
            }
        }
        file = root.openNextFile();
    }
    root.close();
}

void PhotoCarousel::drawImage(int index) {
    if (fallbackMode || imageCount == 0 || index < 0 || index >= imageCount) return;

    gfx->fillScreen(MAT_BG);
    String fullPath = "/" + imageFiles[index];
    int rc = png->open(fullPath.c_str(), pngOpen, pngClose, pngRead, pngSeek, pngDraw);
    if (rc == PNG_SUCCESS) {
        png->decode(NULL, 0);
        png->close();
    }
}

void PhotoCarousel::drawFallback() {
    gfx->fillScreen(MAT_BG);
    int h = gfx->height();
    int step = h / 6;
    uint16_t colors[] = {BGR_RED, BGR_GREEN, BGR_BLUE, BGR_YELLOW, BGR_WHITE, BGR_BLACK};
    for (int i = 0; i < 6; i++) {
        gfx->fillRect(0, i * step, gfx->width(), step, colors[i % 6]);
    }
    gfx->setTextSize(2);
    gfx->setTextColor(BGR_WHITE, BGR_BLACK);
}

void PhotoCarousel::init() {
    loadImages();
    if (imageCount == 0) {
        fallbackMode = true;
        drawFallback();
    } else {
        fallbackMode = false;
        currentImage = 0;
        drawImage(0);
    }
}

void PhotoCarousel::update(uint16_t x, uint16_t y, bool screenTouched) {
    if (screenTouched) {
        if (!touching) {
            touching = true;
            touchStartX = x;
            touchStartY = y;
        }
        // (si hay algo que hacer mientras se mantiene pulsado, aquí)
    } else {
        if (touching) {
            touching = false;
            unsigned long now = millis();
            if (now - lastGestureTime > GESTURE_COOLDOWN && !fallbackMode) {
                int deltaX = (int)x - (int)touchStartX;
                if (abs(deltaX) > 30) {
                    if (deltaX > 0) {
                        currentImage = (currentImage + 1) % imageCount;
                    } else {
                        currentImage = (currentImage - 1 + imageCount) % imageCount;
                    }
                    drawImage(currentImage);
                    lastGestureTime = now;
                }
            }
        }
    }
}
