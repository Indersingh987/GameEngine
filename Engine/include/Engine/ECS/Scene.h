#pragma once

#include "Entity.h"
#include "Components.h"
#include <unordered_map>
#include <vector>
#include <box2d/box2d.h>
#include <string>

class Scene {
public:
    Scene();
    ~Scene();

    b2WorldId getPhysicsWorld();
    const std::vector<Entity>& getAllEntities() const;

    Entity createEntity();
    void destroyEntity(Entity entity);
    void saveToFile(const std::string& filepath);
    void loadFromFile(const std::string& filepath);
    void clear();

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
     b2WorldId physicsWorldId;
    Entity nextEntityId = 1;

    std::unordered_map<Entity, TransformComponent> transforms;
    std::unordered_map<Entity, PhysicsComponent> physics;
    std::unordered_map<Entity, SpriteComponent> sprites;
    std::unordered_map<Entity, TagComponent> tags;

     std::vector<Entity> allEntities;
};