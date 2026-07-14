#include "Engine/ECS/Scene.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
std::string bodyTypeToString(BodyType type) {
    switch (type) {
        case BodyType::Static:    return "Static";
        case BodyType::Kinematic: return "Kinematic";
        case BodyType::Dynamic:   return "Dynamic";
    }
    return "Dynamic";
}

BodyType bodyTypeFromString(const std::string& str) {
    if (str == "Static") return BodyType::Static;
    if (str == "Kinematic") return BodyType::Kinematic;
    return BodyType::Dynamic;
}
}

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
Entity Scene::findEntityByRole(const std::string& role) {
    for (Entity entity : allEntities) {
        if (hasComponent<TagComponent>(entity) && getComponent<TagComponent>(entity).role == role) {
            return entity;
        }
    }
    return INVALID_ENTITY;
}

void Scene::createPhysicsBody(Entity entity) {
    auto& transform = getComponent<TransformComponent>(entity);
    auto& phys = getComponent<PhysicsComponent>(entity);

    b2BodyDef bodyDef = b2DefaultBodyDef();
    switch (phys.bodyType) {
        case BodyType::Static:    bodyDef.type = b2_staticBody;    break;
        case BodyType::Kinematic: bodyDef.type = b2_kinematicBody; break;
        case BodyType::Dynamic:   bodyDef.type = b2_dynamicBody;   break;
    }
    bodyDef.position.x = (transform.x + transform.width / 2.0f) / PIXELS_PER_METER;
    bodyDef.position.y = (transform.y + transform.height / 2.0f) / PIXELS_PER_METER;
    bodyDef.gravityScale = phys.gravityScale;

    b2BodyId bodyId = b2CreateBody(physicsWorldId, &bodyDef);

    b2Polygon box = b2MakeBox(
        (transform.width / PIXELS_PER_METER) / 2.0f,
        (transform.height / PIXELS_PER_METER) / 2.0f
    );
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.0f;
    shapeDef.material.friction = 0.3f;
    b2CreatePolygonShape(bodyId, &shapeDef, &box);

    phys.bodyId = bodyId;
}

void Scene::destroyEntity(Entity entity) {
    auto physicsIt = physics.find(entity);
    if (physicsIt != physics.end() && !B2_IS_NULL(physicsIt->second.bodyId)) {
        b2DestroyBody(physicsIt->second.bodyId);
    }

    transforms.erase(entity);
    physics.erase(entity);
    sprites.erase(entity);
    tags.erase(entity);
    scripts.erase(entity);

    allEntities.erase(
        std::remove(allEntities.begin(), allEntities.end(), entity),
        allEntities.end()
    );
}

nlohmann::json Scene::serializeToJson() {
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
                {"r", s.r}, {"g", s.g}, {"b", s.b}, {"a", s.a}, {"texturePath", s.texturePath}
            };
        }
        if (hasComponent<PhysicsComponent>(entity)) {
            auto& p = getComponent<PhysicsComponent>(entity);
            entityJson["PhysicsComponent"] = {
                {"bodyType", bodyTypeToString(p.bodyType)},
                {"gravityScale", p.gravityScale}
            };
        }
        if (hasComponent<TagComponent>(entity)) {
            auto& tag = getComponent<TagComponent>(entity);
            entityJson["TagComponent"] = {
                {"displayName", tag.displayName},
                {"role", tag.role}
            };
        }
        json["entities"].push_back(entityJson);
    }

    return json;
}

void Scene::deserializeFromJson(const nlohmann::json& json) {
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
            if (entityJson["SpriteComponent"].contains("texturePath")) {
                s.texturePath = entityJson["SpriteComponent"]["texturePath"];
            }
            addComponent<SpriteComponent>(entity, s);
        }
        if (entityJson.contains("PhysicsComponent")) {
            PhysicsComponent p;
            p.bodyType = bodyTypeFromString(entityJson["PhysicsComponent"]["bodyType"]);
            p.gravityScale = entityJson["PhysicsComponent"]["gravityScale"];
            addComponent<PhysicsComponent>(entity, p);
        }
        if (entityJson.contains("TagComponent")) {
            TagComponent tag;
            tag.displayName = entityJson["TagComponent"]["displayName"];
            tag.role = entityJson["TagComponent"]["role"];
            addComponent<TagComponent>(entity, tag);
        }
    }
}

void Scene::saveToFile(const std::string& filepath) {
    std::ofstream file(filepath);
    file << serializeToJson().dump(4);
}

void Scene::loadFromFile(const std::string& filepath) {
    clear();
    std::ifstream file(filepath);
    nlohmann::json json;
    file >> json;
    deserializeFromJson(json);
}

nlohmann::json Scene::captureSnapshot() {
    return serializeToJson();
}

void Scene::restoreSnapshot(const nlohmann::json& snapshot) {
    clear();
    deserializeFromJson(snapshot);
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
    tags.clear();
    scripts.clear();
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

template<>
std::unordered_map<Entity, TagComponent>& Scene::getStorage<TagComponent>() {
    return tags;
}

template<>
std::unordered_map<Entity, ScriptComponent>& Scene::getStorage<ScriptComponent>() {
    return scripts;
}