#pragma once
#include <stdint.h>

enum class RenderLayer : uint8_t {
    Background,
    World,
    Characters,
    Effects,
    UI,
    Popup,
    Debug,
    Count // Utilizado para saber el tamaño de las capas
};
