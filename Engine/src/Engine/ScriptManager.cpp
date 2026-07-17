#include "Engine/ScriptManager.h"
#include "Engine/TierScript.h"
#include <box2d/box2d.h>
#include <charconv>
#include <filesystem>
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

bool ScriptManager::parseScript(const std::string& scriptPath, ScriptData& outData) {
    // A missing file hits luaL_loadfile's C-level open failure, which happens outside any
    // protected-call context sol2 can catch as a normal protected_function_result - it falls
    // through to sol2's default panic handler and aborts the process instead. Guard against that
    // specific case here rather than relying on the protected-call check below.
    if (!std::filesystem::exists(scriptPath)) {
        std::cerr << "Failed to load script '" << scriptPath << "': file not found" << std::endl;
        return false;
    }

    ScriptData data;
    data.env = sol::environment(lua, sol::new_table(), lua.globals());

    // MUST pass sol::script_pass_on_error explicitly. script_file()'s DEFAULT error handler
    // (script_default_on_error, used if this 3rd arg is omitted) throws a C++ sol::error on any
    // load-time failure - including a plain Lua SYNTAX error in a file that opened fine, not just
    // the missing-file case guarded above - because this vcpkg sol2 build has SOL_EXCEPTIONS on
    // and SOL_DEFAULT_PASS_ON_ERROR off (confirmed by reading the installed
    // sol/state_handling.hpp). Nothing in this call chain catches sol::error, so that throw was
    // an unhandled-exception crash waiting to happen on any bad script, not just this feature's
    // new Save button. script_pass_on_error instead just returns the invalid
    // protected_function_result, letting the check below handle it the way the rest of this
    // function already assumes.
    sol::protected_function_result result = lua.script_file(scriptPath, data.env, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Failed to load script '" << scriptPath << "': " << err.what() << std::endl;
        return false;
    }

    data.onUpdate = data.env["onUpdate"];
    data.onCollision = data.env["onCollision"];

    outData = std::move(data);
    return true;
}

ScriptData* ScriptManager::loadScript(const std::string& scriptPath) {
    auto it = cache.find(scriptPath);
    if (it != cache.end()) {
        return &it->second;
    }

    ScriptData data;
    if (!parseScript(scriptPath, data)) {
        return nullptr;
    }

    auto [insertedIt, inserted] = cache.emplace(scriptPath, std::move(data));
    return &insertedIt->second;
}

bool ScriptManager::reloadScript(const std::string& scriptPath) {
    ScriptData data;
    if (!parseScript(scriptPath, data)) {
        return false;
    }

    auto it = cache.find(scriptPath);
    if (it != cache.end()) {
        // Assign into the existing slot rather than erase+emplace - keeps the slot's address
        // stable so any raw ScriptData* already handed out (e.g. TierScript::data, though
        // TierScripts should not go through this path - see the header comment) stays valid.
        it->second = std::move(data);
    } else {
        cache.emplace(scriptPath, std::move(data));
    }
    return true;
}

void ScriptManager::clearCache() {
    cache.clear();
}

bool ScriptManager::checkSyntax(const std::string& code, const std::string& chunkName, std::string& outError, int& outErrorLine) {
    // Lua's chunk-name convention (luaO_chunkid, confirmed by reading the actual vendored Lua
    // source in external/vcpkg/buildtrees/lua): a bare chunk name with no '@'/'=' prefix is
    // treated as a literal source STRING and gets wrapped as `[string "chunkName"]` in error
    // messages, not shown as chunkName itself - the '@' prefix is what tells Lua to treat it as
    // a filename and display it clean. Without this, the prefix match below never matches
    // anything and outErrorLine silently stays stuck at 1 regardless of the real error line.
    sol::load_result result = lua.load(code, "@" + chunkName);
    if (result.valid()) {
        return true;
    }

    sol::error err = result;
    outError = err.what();

    // Lua's own syntax errors are formatted "chunkname:line: message" - pull the line back out
    // so the editor can place an ErrorMarker on it. Falls back to line 1 if the message doesn't
    // match that format (defensive only - lua_load's syntax errors always do).
    outErrorLine = 1;
    std::string prefix = chunkName + ":";
    if (outError.rfind(prefix, 0) == 0) {
        std::size_t lineStart = prefix.size();
        std::size_t lineEnd = outError.find(':', lineStart);
        if (lineEnd != std::string::npos) {
            int parsedLine = 0;
            auto [ptr, ec] = std::from_chars(outError.data() + lineStart, outError.data() + lineEnd, parsedLine);
            if (ec == std::errc()) {
                outErrorLine = parsedLine;
            }
        }
    }
    return false;
}
