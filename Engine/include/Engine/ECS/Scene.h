#pragma once

#include "Entity.h"
#include "Components.h"
#include <unordered_map>
#include <vector>

class Scene {
public:
    Entity createEntity();
    void destroyEntity(Entity entity);

    template<typename T>
    void addComponent(Entity entity, T component) {
        getStorage<T>()[entity] = component;
    }

    template<typename T>
    T& getComponent(Entity entity) {
        return getStorage<T>().at(entity);
    }

    template<typename T>
    bool hasComponent(Entity entity) {
        auto& storage = getStorage<T>();
        return storage.find(entity) != storage.end();
    }

    template<typename T>
    std::unordered_map<Entity, T>& getStorage();

private:
    Entity nextEntityId = 1;

    std::unordered_map<Entity, TransformComponent> transforms;
    std::unordered_map<Entity, VelocityComponent> velocities;
    std::unordered_map<Entity, PhysicsComponent> physics;
    std::unordered_map<Entity, SpriteComponent> sprites;
};