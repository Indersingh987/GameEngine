#include "Engine/ScriptManager.h"
#include <box2d/box2d.h>
#include <iostream>
#include <tuple>

namespace {
BodyType bodyTypeFromString(const std::string& str) {
    if (str == "Static") return BodyType::Static;
    if (str == "Kinematic") return BodyType::Kinematic;
    return BodyType::Dynamic;
}
}

ScriptManager::ScriptManager(Scene& sceneRef, AudioManager& audioRef)
    : scene(sceneRef), audio(audioRef) {
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
    registerBindings();
}

void ScriptManager::registerBindings() {
    lua.set_function("getPosition", [this](Entity entity) {
        auto& t = scene.getComponent<TransformComponent>(entity);
        return std::make_tuple(t.x, t.y);
    });

    lua.set_function("setPosition", [this](Entity entity, float x, float y) {
        auto& t = scene.getComponent<TransformComponent>(entity);
        if (scene.hasComponent<PhysicsComponent>(entity)) {
            auto& phys = scene.getComponent<PhysicsComponent>(entity);
            if (!B2_IS_NULL(phys.bodyId)) {
                b2Vec2 center = {
                    (x + t.width / 2.0f) / PIXELS_PER_METER,
                    (y + t.height / 2.0f) / PIXELS_PER_METER
                };
                b2Body_SetTransform(phys.bodyId, center, b2Body_GetRotation(phys.bodyId));
                return;
            }
        }
        // No physics body to be the source of truth for - just move the transform directly.
        t.x = x;
        t.y = y;
    });

    lua.set_function("getVelocity", [this](Entity entity) {
        auto& phys = scene.getComponent<PhysicsComponent>(entity);
        b2Vec2 v = B2_IS_NULL(phys.bodyId) ? b2Vec2{0.0f, 0.0f} : b2Body_GetLinearVelocity(phys.bodyId);
        return std::make_tuple(v.x, v.y);
    });

    lua.set_function("setVelocity", [this](Entity entity, float vx, float vy) {
        auto& phys = scene.getComponent<PhysicsComponent>(entity);
        if (!B2_IS_NULL(phys.bodyId)) {
            b2Body_SetLinearVelocity(phys.bodyId, b2Vec2{vx, vy});
        }
    });

    lua.set_function("spawnEntity", [this](float x, float y, int width, int height, const std::string& bodyTypeStr) {
        Entity entity = scene.createEntity();
        scene.addComponent<TransformComponent>(entity, {x, y, width, height});
        scene.addComponent<PhysicsComponent>(entity, {b2_nullBodyId, bodyTypeFromString(bodyTypeStr), 0.0f});
        scene.createPhysicsBody(entity);
        return entity;
    });

    lua.set_function("destroyEntity", [this](Entity entity) {
        scene.destroyEntity(entity);
    });

    lua.set_function("playSound", [this](const std::string& name) {
        audio.playSound(name);
    });
}

ScriptData* ScriptManager::loadScript(const std::string& scriptPath) {
    auto it = cache.find(scriptPath);
    if (it != cache.end()) {
        return &it->second;
    }

    ScriptData data;
    data.env = sol::environment(lua, sol::new_table(), lua.globals());

    sol::protected_function_result result = lua.script_file(scriptPath, data.env);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Failed to load script '" << scriptPath << "': " << err.what() << std::endl;
        return nullptr;
    }

    data.onUpdate = data.env["onUpdate"];
    data.onCollision = data.env["onCollision"];

    auto [insertedIt, inserted] = cache.emplace(scriptPath, std::move(data));
    return &insertedIt->second;
}

void ScriptManager::clearCache() {
    cache.clear();
}
