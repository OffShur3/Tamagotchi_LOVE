#pragma once
#include "RenderContext.h"
#include "RenderLayer.h"

class RenderObject {
public:
    RenderObject() 
        : position{0, 0}, scale{1, 1}, rotation(0.0f), visible(true), layer(RenderLayer::World), dirty(true) {}
    
    virtual ~RenderObject() = default;

    virtual void draw(RenderContext& ctx) = 0;
    virtual void update(float dt) {}

    Vector2 position;
    Vector2 scale;
    float rotation;
    bool visible;
    RenderLayer layer;
    bool dirty;

    virtual Size2 getSize() const = 0;
};
