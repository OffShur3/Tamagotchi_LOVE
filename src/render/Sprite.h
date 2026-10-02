// src/render/Sprite.h
#pragma once
#include "RenderObject.h"
#include "../assets/Texture.h"
#include <memory>

class Sprite : public RenderObject {
public:
    std::shared_ptr<Texture> texture;
    Vector2 frameOffset = {0, 0};
    Size2 frameSize = {0, 0};
    bool flipX = false;
    bool flipY = false;

    // --- EFECTO GBA DE SILUETA / EVOLUCIÓN ---
    bool silhouetteMode = false;
    uint16_t silhouetteColor = 0xFFFF; // Blanco puro por defecto

    Sprite(std::shared_ptr<Texture> tex = nullptr);
    ~Sprite() override = default;

    Size2 getSize() const override;
    void draw(RenderContext& ctx) override;
};
