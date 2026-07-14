#pragma once

#include "Engine/ECS/Scene.h"
#include "Engine/AudioManager.h"
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

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

private:
    Scene& scene;
    AudioManager& audio;
    sol::state lua;
    std::unordered_map<std::string, ScriptData> cache;

    void registerBindings();
};
