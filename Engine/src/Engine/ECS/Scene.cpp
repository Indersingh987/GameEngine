#include "Engine/ECS/Scene.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

Scene::Scene() {
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity.x = 0.0f;
    worldDef.gravity.y = 9.8f;
    physicsWorldId = b2CreateWorld(&worldDef);
}

Scene::~Scene() {
    b2DestroyWorld(physicsWorldId);
}

b2WorldId Scene::getPhysicsWorld() {
    return physicsWorldId;
}
Entity Scene::createEntity() {
    Entity newEntity = nextEntityId++;
    allEntities.push_back(newEntity);
    return newEntity;
}
const std::vector<Entity>& Scene::getAllEntities() const {
    return allEntities;
}
void Scene::destroyEntity(Entity entity) {
    transforms.erase(entity);
    physics.erase(entity);
    sprites.erase(entity);

    allEntities.erase(
        std::remove(allEntities.begin(), allEntities.end(), entity),
        allEntities.end()
    );
}

void Scene::saveToFile(const std::string& filepath) {
    nlohmann::json json;
    json["entities"] = nlohmann::json::array();

    for (Entity entity : allEntities) {
        nlohmann::json entityJson;
        entityJson["id"] = entity;

        if (hasComponent<TransformComponent>(entity)) {
            auto& t = getComponent<TransformComponent>(entity);
            entityJson["TransformComponent"] = {
                {"x", t.x}, {"y", t.y}, {"width", t.width}, {"height", t.height}
            };
        }

        if (hasComponent<SpriteComponent>(entity)) {
            auto& s = getComponent<SpriteComponent>(entity);
            entityJson["SpriteComponent"] = {
                {"r", s.r}, {"g", s.g}, {"b", s.b}, {"a", s.a}
            };
        }
        if (hasComponent<PhysicsComponent>(entity)) {
            auto& p = getComponent<PhysicsComponent>(entity);
            entityJson["PhysicsComponent"] = {
                {"isDynamic", p.isDynamic},
                {"gravityScale", p.gravityScale}
            };
        }
        json["entities"].push_back(entityJson);
    }

    std::ofstream file(filepath);
    file << json.dump(4);
}

void Scene::loadFromFile(const std::string& filepath) {
     clear();
    std::ifstream file(filepath);
    nlohmann::json json;
    file >> json;

    for (auto& entityJson : json["entities"]) {
        Entity entity = createEntity();

        if (entityJson.contains("TransformComponent")) {
            TransformComponent t;
            t.x = entityJson["TransformComponent"]["x"];
            t.y = entityJson["TransformComponent"]["y"];
            t.width = entityJson["TransformComponent"]["width"];
            t.height = entityJson["TransformComponent"]["height"];
            addComponent<TransformComponent>(entity, t);
        }

        if (entityJson.contains("SpriteComponent")) {
            SpriteComponent s;
            s.r = entityJson["SpriteComponent"]["r"];
            s.g = entityJson["SpriteComponent"]["g"];
            s.b = entityJson["SpriteComponent"]["b"];
            s.a = entityJson["SpriteComponent"]["a"];
            addComponent<SpriteComponent>(entity, s);
        }
        if (entityJson.contains("PhysicsComponent")) {
            PhysicsComponent p;
            p.isDynamic = entityJson["PhysicsComponent"]["isDynamic"];
            p.gravityScale = entityJson["PhysicsComponent"]["gravityScale"];
            addComponent<PhysicsComponent>(entity, p);
        }
    }
}

void Scene::clear() {
    

    for (auto& pair : physics) {
        if (!B2_IS_NULL(pair.second.bodyId)) {
            b2DestroyBody(pair.second.bodyId);
        }
    }

    transforms.clear();
    physics.clear();
    sprites.clear();
    allEntities.clear();
    nextEntityId = 1;
}



// Template specializations — tell the compiler which storage map corresponds to which component type
template<>
std::unordered_map<Entity, TransformComponent>& Scene::getStorage<TransformComponent>() {
    return transforms;
}

template<>
std::unordered_map<Entity, PhysicsComponent>& Scene::getStorage<PhysicsComponent>() {
    return physics;
}

template<>
std::unordered_map<Entity, SpriteComponent>& Scene::getStorage<SpriteComponent>() {
    return sprites;
}