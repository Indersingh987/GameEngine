#include "Engine/ScriptManager.h"
#include "Engine/TierScript.h"
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

    lua.set_function("setColor", [this](Entity entity, int r, int g, int b, int a) {
        if (!scene.hasComponent<SpriteComponent>(entity)) {
            scene.addComponent<SpriteComponent>(entity, SpriteComponent{});
        }
        auto& sprite = scene.getComponent<SpriteComponent>(entity);
        sprite.r = static_cast<Uint8>(r);
        sprite.g = static_cast<Uint8>(g);
        sprite.b = static_cast<Uint8>(b);
        sprite.a = static_cast<Uint8>(a);
    });

    // keyName follows SDL_GetScancodeFromName's naming (e.g. "Space", "A", "Left"), same
    // SDL_GetKeyboardState array handleInput() reads from - just called fresh here since
    // scripts don't have a per-frame keystate pointer threaded in.
    lua.set_function("isKeyPressed", [](const std::string& keyName) {
        SDL_Scancode scancode = SDL_GetScancodeFromName(keyName.c_str());
        if (scancode == SDL_SCANCODE_UNKNOWN) {
            return false;
        }
        const Uint8* keystate = SDL_GetKeyboardState(nullptr);
        return keystate[scancode] != 0;
    });

    // Thin wrapper over Scene::findEntityByRole (already used internally for player lookup).
    // Returns INVALID_ENTITY (0) if no entity currently holds that role.
    lua.set_function("getEntityByRole", [this](const std::string& role) {
        return scene.findEntityByRole(role);
    });

    // Default callScript, visible to every entity script (via globals fallback) unless a more
    // specific tier overrides it locally - entity scripts may only ever target "scene", per the
    // hierarchy's communication rule (see CLAUDE.md CURRENT FEATURE SPEC, Part A). Scene/Game
    // scripts get their own tier-scoped callScript bound directly into their environment by
    // TierScript::bindCallScript, which shadows this one.
    lua.set_function("callScript", [this](const std::string& targetTier, const std::string& functionName, sol::variadic_args args) -> sol::object {
        if (targetTier != "scene") {
            std::cerr << "callScript: entity scripts may only target \"scene\" (got \"" << targetTier << "\")" << std::endl;
            return sol::lua_nil;
        }
        if (sceneScript == nullptr) {
            std::cerr << "callScript: no scene script is loaded" << std::endl;
            return sol::lua_nil;
        }
        return sceneScript->call(functionName, args);
    });
}

void ScriptManager::setSceneScript(TierScript* sceneScriptPtr) {
    sceneScript = sceneScriptPtr;
}

sol::object ScriptManager::callEntityFunction(Entity entity, const std::string& functionName, const std::vector<sol::object>& args) {
    if (!scene.hasComponent<ScriptComponent>(entity)) {
        std::cerr << "callScript: entity " << entity << " has no ScriptComponent" << std::endl;
        return sol::lua_nil;
    }

    auto& scriptComp = scene.getComponent<ScriptComponent>(entity);
    if (scriptComp.scriptPath.empty()) {
        std::cerr << "callScript: entity " << entity << " has an empty scriptPath" << std::endl;
        return sol::lua_nil;
    }

    ScriptData* data = loadScript(scriptComp.scriptPath);
    if (data == nullptr) {
        return sol::lua_nil;
    }

    // Entity-tier functions (onUpdate, onCollision, and any callScript-exposed function alike)
    // always take the acting entity as their own first argument, since one script file is
    // shared across many entities - re-prepend it here rather than assuming the caller already
    // included it among args.
    std::vector<sol::object> fullArgs;
    fullArgs.reserve(args.size() + 1);
    fullArgs.push_back(sol::make_object(lua, entity));
    fullArgs.insert(fullArgs.end(), args.begin(), args.end());

    return invokeScriptFunction(*data, functionName, sol::as_args(fullArgs));
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
