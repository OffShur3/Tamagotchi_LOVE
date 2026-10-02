// src/render/Sprite.cpp
#include "Sprite.h"
#include "Blend.h"

Sprite::Sprite(std::shared_ptr<Texture> tex) : texture(tex) {
    if (texture) {
        frameSize = { static_cast<int16_t>(texture->width), static_cast<int16_t>(texture->height) };
    }
}

Size2 Sprite::getSize() const {
    return frameSize;
}

void Sprite::draw(RenderContext& ctx) {
    if (!texture || !texture->pixels) return;

    int16_t texWidth = texture->width;
    int16_t texHeight = texture->height;

    int16_t drawW = (frameSize.w > 0) ? frameSize.w : texWidth;
    int16_t drawH = (frameSize.h > 0) ? frameSize.h : texHeight;

    for (int16_t dy = 0; dy < drawH; ++dy) {
        int16_t destY = position.y + dy * scale.y;

        for (int16_t sy = 0; sy < scale.y; ++sy) {
            int16_t finalY = destY + sy;
            if (finalY < 0 || finalY >= ctx.height) continue;

            for (int16_t dx = 0; dx < drawW; ++dx) {
                int16_t destX = position.x + dx * scale.x;

                int16_t srcX = frameOffset.x + (flipX ? (drawW - 1 - dx) : dx);
                int16_t srcY = frameOffset.y + (flipY ? (drawH - 1 - dy) : dy);

                if (srcX < 0 || srcX >= texWidth || srcY < 0 || srcY >= texHeight) continue;

                uint32_t srcIdx = srcY * texWidth + srcX;
                
                // MODO SILUETA GBA: Si está activo, forzar el color de brillo blanco/gris
                uint16_t color = silhouetteMode ? silhouetteColor : texture->pixels[srcIdx];
                uint8_t alpha = texture->alpha ? texture->alpha[srcIdx] : 255;

                if (alpha == 0) continue; 

                for (int16_t sx = 0; sx < scale.x; ++sx) {
                    int16_t finalX = destX + sx;
                    if (finalX < 0 || finalX >= ctx.width) continue; 

                    if (alpha == 255) {
                        ctx.framebuffer[finalY * ctx.width + finalX] = color;
                    } else {
                        uint16_t bgColor = ctx.framebuffer[finalY * ctx.width + finalX];
                        ctx.framebuffer[finalY * ctx.width + finalX] = blendRGB565(bgColor, color, alpha);
                    }
                }
            }
        }
    }
}
