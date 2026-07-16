#pragma once

#include "Engine/ScriptManager.h"
#include <functional>
#include <iostream>
#include <string>
#include <utility>

// Protected-calls the named function on a loaded script's environment, if it defines one. Logs
// to stderr and returns nil on error or if the function isn't defined - the one place this
// "call a named Lua function safely" operation lives, reused by TierScript::call and by
// ScriptManager's entity-targeted callScript dispatch (see TierScript::bindSceneCallScript).
// Templated so callers can forward either a live sol::variadic_args (spreads the calling
// function's own trailing args) or a sol::as_args(vector) (spreads an assembled arg list).
template<typename Args>
sol::object invokeScriptFunction(ScriptData& data, const std::string& functionName, Args&& args) {
    sol::protected_function fn = data.env[functionName];
    if (!fn.valid()) {
        return sol::lua_nil;
    }

    sol::protected_function_result result = fn(std::forward<Args>(args));
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Script error calling '" << functionName << "': " << err.what() << std::endl;
        return sol::lua_nil;
    }

    return result.get<sol::object>(0);
}

// One loaded script instance for the Game or Scene tier, as opposed to entity-tier scripts
// (which stay in ScriptManager's path-keyed cache since many entities can share one path).
// GameScript (one per app, owned by GameplayScene) and SceneScript (one per Scene, owned by
// Scene) are both this same shape - see CLAUDE.md's CURRENT FEATURE SPEC, Part A.
class TierScript {
public:
    bool load(const std::string& scriptPath, ScriptManager& scriptManager);
    bool isLoaded() const;

    // Protected-call into the named function this script defined, if any. Logs to stderr and
    // returns nil on error or if the function isn't defined - mirrors onUpdate's error handling
    // in Systems::script.
    sol::object call(const std::string& functionName, sol::variadic_args args);

    // Binds a callScript(targetTier, functionName, ...) function into this script's OWN
    // environment, restricted to only ever target one specific neighboring tier - this is how
    // the hierarchy's up/down direction rules are enforced structurally (Scene -> "game" only,
    // Game -> "scene" only) rather than by tracking caller identity at runtime. Use this for
    // symmetric single-target tiers; Scene needs bindSceneCallScript instead, since it mediates
    // both the Game script AND its own entities.
    void bindCallScript(const std::string& allowedTargetTier, TierScript& target);

    // Scene is the one tier with two allowed targets: callScript("game", fn, ...) reaches the
    // Game script (upward), callScript("entity", fn, entityId, ...) reaches a specific entity's
    // own script (downward, mediating it) - entityId is always the first arg after fn, the rest
    // are forwarded to that entity's function. Entity <-> Entity direct calls stay impossible:
    // this is the only place an "entity" target is ever accepted.
    void bindSceneCallScript(TierScript& gameScript, ScriptManager& scriptManager);

    // Binds a switchScene(scenePath, sceneScriptPath) -> bool function into this script's OWN
    // environment only - never call this for sceneScript or any entity script. This is the
    // structural enforcement that only the Game tier may load/switch scenes (same technique as
    // bindCallScript's direction rules: a script simply doesn't have a binding to a forbidden
    // capability, rather than a runtime caller-identity check).
    void bindSwitchScene(std::function<bool(const std::string&, const std::string&)> switchFn);

private:
    ScriptData* data = nullptr;
};
