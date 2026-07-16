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

    // Recorrido de pixeles y escalado sin interpolación suavizada (Perfect Pixel Art Scaling)
    for (int16_t dy = 0; dy < drawH; ++dy) {
        int16_t destY = position.y + dy * scale.y;

        for (int16_t sy = 0; sy < scale.y; ++sy) {
            int16_t finalY = destY + sy;
            if (finalY < 0 || finalY >= ctx.height) continue; // Recorte superior/inferior

            for (int16_t dx = 0; dx < drawW; ++dx) {
                int16_t destX = position.x + dx * scale.x;

                // Aplicar espejado (flip)
                int16_t srcX = frameOffset.x + (flipX ? (drawW - 1 - dx) : dx);
                int16_t srcY = frameOffset.y + (flipY ? (drawH - 1 - dy) : dy);

                // Evitar lecturas fuera de límites de la textura física
                if (srcX < 0 || srcX >= texWidth || srcY < 0 || srcY >= texHeight) continue;

                uint32_t srcIdx = srcY * texWidth + srcX;
                uint16_t color = texture->pixels[srcIdx];
                uint8_t alpha = texture->alpha ? texture->alpha[srcIdx] : 255;

                if (alpha == 0) continue; // Completamente transparente

                for (int16_t sx = 0; sx < scale.x; ++sx) {
                    int16_t finalX = destX + sx;
                    if (finalX < 0 || finalX >= ctx.width) continue; // Recorte lateral

                    if (alpha == 255) {
                        ctx.framebuffer[finalY * ctx.width + finalX] = color;
                    } else {
                        // Mezcla alfa con el fondo existente del Framebuffer
                        uint16_t bgColor = ctx.framebuffer[finalY * ctx.width + finalX];
                        ctx.framebuffer[finalY * ctx.width + finalX] = blendRGB565(bgColor, color, alpha);
                    }
                }
            }
        }
    }
}
