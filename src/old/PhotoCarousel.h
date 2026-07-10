#ifndef PHOTO_CAROUSEL_H
#define PHOTO_CAROUSEL_H

#include "Game.h"
#include <Arduino_GFX_Library.h>
#include <PNGdec.h>
#include <SD_MMC.h>
#include <FS.h>

class PhotoCarousel : public Game {
public:
    PhotoCarousel(Arduino_GFX* gfx, PNG* png, 
                  void* (*openFn)(const char*, int32_t*),
                  void (*closeFn)(void*),
                  int32_t (*readFn)(PNGFILE*, uint8_t*, int32_t),
                  int32_t (*seekFn)(PNGFILE*, int32_t),
                  int (*drawFn)(PNGDRAW*));
    
    void init() override;
    void update(uint16_t x, uint16_t y, bool touched) override;
    void redraw() override;

private:
    Arduino_GFX* gfx;
    PNG* png;
    void* (*pngOpen)(const char*, int32_t*);
    void (*pngClose)(void*);
    int32_t (*pngRead)(PNGFILE*, uint8_t*, int32_t);
    int32_t (*pngSeek)(PNGFILE*, int32_t);
    int (*pngDraw)(PNGDRAW*);

    static const int MAX_IMAGES = 20;
    String imageFiles[MAX_IMAGES];
    int imageCount = 0;
    int currentImage = 0;
    bool fallbackMode = false;

    uint16_t touchStartX = 0, touchStartY = 0;
    bool touching = false;
    unsigned long lastGestureTime = 0;
    static const unsigned long GESTURE_COOLDOWN = 200;

    void drawImage(int index);
    void drawFallback();
    void loadImages();
};

#endif
