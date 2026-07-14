#pragma once

#include "Scene.h"
#include "Engine/ScriptManager.h"
#include <SDL2/SDL.h>

namespace Systems {
    void physics(Scene& scene, Entity entity, float deltaTime);
    void render(Scene& scene, Entity entity, SDL_Renderer* renderer);
    void script(Scene& scene, Entity entity, float deltaTime, ScriptManager& scriptManager);
}