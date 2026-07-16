#pragma once
#include <vector>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <memory>

class Event {
public:
    virtual ~Event() = default;
};

class EventBus {
public:
    using EventCallback = std::function<void(const Event&)>;

    static EventBus& getInstance() {
        static EventBus instance;
        return instance;
    }

    template<typename T>
    void subscribe(std::function<void(const T&)> callback) {
        auto type = std::type_index(typeid(T));
        subscribers[type].push_back([callback](const Event& event) {
            callback(static_cast<const T&>(event));
        });
    }

    template<typename T, typename... Args>
    void publish(Args&&... args) {
        auto type = std::type_index(typeid(T));
        if (subscribers.find(type) != subscribers.end()) {
            T event(std::forward<Args>(args)...);
            for (auto& callback : subscribers[type]) {
                callback(event);
            }
        }
    }

private:
    EventBus() = default;
    std::unordered_map<std::type_index, std::vector<EventCallback>> subscribers;
};
