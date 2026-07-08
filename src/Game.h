#ifndef GAME_H
#define GAME_H

#include <Arduino.h>

class Game {
public:
    virtual ~Game() {}
    virtual void init() = 0;
    virtual void update(uint16_t x, uint16_t y, bool touched) = 0;
    virtual void redraw() = 0;
};

#endif
