// src/render/Scene.h
#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "RenderObject.h"

class Scene {
public:
    virtual ~Scene() = default;
    
    virtual void enter() = 0;
    virtual void exit() = 0;
    virtual void update(float dt) = 0;

    virtual void onTouch(uint16_t x, uint16_t y) {}
    virtual void onTouchReleased() {}

    const std::vector<std::shared_ptr<RenderObject>>& getObjects() const { return objects; }

protected:
    void addObject(std::shared_ptr<RenderObject> obj) {
        objects.push_back(obj);
    }

    void removeObject(std::shared_ptr<RenderObject> obj) {
        objects.erase(std::remove(objects.begin(), objects.end(), obj), objects.end());
    }

    void clearObjects() {
        objects.clear();
    }

private:
    std::vector<std::shared_ptr<RenderObject>> objects;
};
