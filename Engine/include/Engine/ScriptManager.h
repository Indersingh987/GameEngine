#pragma once

#include "Engine/ECS/Scene.h"
#include "Engine/AudioManager.h"
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class TierScript;

// Cached, parsed form of one .lua file. Multiple entities sharing the same scriptPath
// (e.g. every pipe using pipe_scroll.lua) share one of these - the hooks take the acting
// entity as a parameter rather than closing over it.
struct ScriptData {
    sol::environment env;
    sol::protected_function onUpdate;    // invalid/nil if the script doesn't define one
    sol::protected_function onCollision; // invalid/nil if the script doesn't define one
};

class ScriptManager {
public:
    ScriptManager(Scene& scene, AudioManager& audio);

    // Loads and caches the script at scriptPath (returns the cached entry on repeat calls).
    // Returns nullptr if the file fails to load/parse; logs the error to stderr.
    ScriptData* loadScript(const std::string& scriptPath);

    void clearCache();

    // Entity scripts' callScript("scene", ...) default resolves to whatever Scene script is
    // currently loaded - set once at startup (see GameplayScene's constructor), re-set if a
    // scene switch ever loads a different Scene script.
    void setSceneScript(TierScript* sceneScriptPtr);

    // Resolves entity's own ScriptComponent and protected-calls functionName on it, forwarding
    // args. Backs the Scene tier's callScript("entity", fn, entityId, ...) - the one path a
    // Scene script has to reach into a specific entity's own script. Logs to stderr and returns
    // nil if the entity has no ScriptComponent, an empty scriptPath, or the function errors.
    sol::object callEntityFunction(Entity entity, const std::string& functionName, const std::vector<sol::object>& args);

private:
    Scene& scene;
    AudioManager& audio;
    sol::state lua;
    std::unordered_map<std::string, ScriptData> cache;
    TierScript* sceneScript = nullptr;

    void registerBindings();
};
