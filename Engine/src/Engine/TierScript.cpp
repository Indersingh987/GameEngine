#include "Engine/TierScript.h"
#include <iostream>
#include <vector>

bool TierScript::load(const std::string& scriptPath, ScriptManager& scriptManager) {
    data = scriptManager.loadScript(scriptPath);
    return data != nullptr;
}

bool TierScript::isLoaded() const {
    return data != nullptr;
}

sol::object TierScript::call(const std::string& functionName, sol::variadic_args args) {
    if (data == nullptr) {
        return sol::lua_nil;
    }
    return invokeScriptFunction(*data, functionName, args);
}

void TierScript::bindCallScript(const std::string& allowedTargetTier, TierScript& target) {
    if (data == nullptr) {
        return;
    }

    data->env.set_function("callScript",
        [&target, allowedTargetTier](const std::string& targetTier, const std::string& functionName, sol::variadic_args args) -> sol::object {
            if (targetTier != allowedTargetTier) {
                std::cerr << "callScript: this script may only target \"" << allowedTargetTier
                           << "\" (got \"" << targetTier << "\")" << std::endl;
                return sol::lua_nil;
            }
            return target.call(functionName, args);
        });
}

void TierScript::bindSceneCallScript(TierScript& gameScript, ScriptManager& scriptManager) {
    if (data == nullptr) {
        return;
    }

    data->env.set_function("callScript",
        [&gameScript, &scriptManager](const std::string& targetTier, const std::string& functionName, sol::variadic_args args) -> sol::object {
            if (targetTier == "game") {
                return gameScript.call(functionName, args);
            }

            if (targetTier == "entity") {
                if (args.size() < 1) {
                    std::cerr << "callScript: target \"entity\" needs an entity id as the first arg after the function name" << std::endl;
                    return sol::lua_nil;
                }
                Entity entity = args[0].get<Entity>();
                std::vector<sol::object> rest;
                for (std::size_t i = 1; i < args.size(); ++i) {
                    rest.push_back(args[i].get<sol::object>());
                }
                return scriptManager.callEntityFunction(entity, functionName, rest);
            }

            std::cerr << "callScript: scene scripts may only target \"game\" or \"entity\" (got \"" << targetTier << "\")" << std::endl;
            return sol::lua_nil;
        });
}

void TierScript::bindSwitchScene(std::function<bool(const std::string&, const std::string&)> switchFn) {
    if (data == nullptr) {
        return;
    }

    data->env.set_function("switchScene", switchFn);
}
