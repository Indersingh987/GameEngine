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

    // Re-parses the file at scriptPath and overwrites its EXISTING cache slot in place (assigns
    // into the map's existing entry rather than erasing/reinserting), so any raw ScriptData*
    // held elsewhere (e.g. TierScript::data) stays valid and automatically observes the new
    // content on its next use - no separate rebind step needed for entity scripts, since their
    // only bindings (getPosition, setPosition, etc.) live on the shared lua.globals() table, not
    // per-environment. NOT safe to use for a Game/Scene TierScript's own path - those scripts get
    // extra bindings (callScript, switchScene) installed into their OWN environment at load() time,
    // which this function does not redo; reload a TierScript via its own load() instead. Fails
    // soft: on a missing file or parse error, logs to stderr, leaves any existing cached version
    // untouched, and returns false. If scriptPath was never cached, behaves like a fresh load.
    bool reloadScript(const std::string& scriptPath);

    void clearCache();

    // Pure syntax check: compiles code as a chunk named chunkName via sol2's raw load(),
    // WITHOUT running it and without touching the cache - used by the script editor's Save
    // action to catch a syntax mistake before it's ever written to disk, rather than after a
    // failed reload. Returns true if syntactically valid; otherwise fills outError/outErrorLine
    // (1-indexed, for TextEditor::SetErrorMarkers) from Lua's own "chunkname:line: message"
    // error format and returns false.
    bool checkSyntax(const std::string& code, const std::string& chunkName, std::string& outError, int& outErrorLine);

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

    // Shared parse step behind loadScript/reloadScript: opens+runs scriptPath into a fresh
    // ScriptData (new environment, onUpdate/onCollision resolved). Returns false, leaving
    // outData untouched, on a missing file or Lua error - logs to stderr either way.
    bool parseScript(const std::string& scriptPath, ScriptData& outData);
};
