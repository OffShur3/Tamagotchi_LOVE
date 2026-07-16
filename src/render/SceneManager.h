#pragma once
#include "Scene.h"
#include <memory>

class SceneManager {
public:
    static SceneManager& getInstance() {
        static SceneManager instance;
        return instance;
    }

    void changeScene(std::shared_ptr<Scene> newScene) {
        if (currentScene) {
            currentScene->exit();
        }
        currentScene = newScene;
        if (currentScene) {
            currentScene->enter();
        }
    }

    void update(float dt) {
        if (currentScene) {
            currentScene->update(dt);
        }
    }

    std::shared_ptr<Scene> getCurrentScene() const {
        return currentScene;
    }

private:
    SceneManager() = default;
    std::shared_ptr<Scene> currentScene = nullptr;
};
