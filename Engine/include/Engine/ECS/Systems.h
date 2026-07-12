#pragma once

#include "Scene.h"
#include <SDL2/SDL.h>

namespace Systems {
    void movement(Scene& scene, Entity entity, float deltaTime);
    void physics(Scene& scene, Entity entity, float deltaTime);
    void render(Scene& scene, Entity entity, SDL_Renderer* renderer);
    bool checkCollision(Scene& scene, Entity a, Entity b);
    void resolveCollision(Scene& scene, Entity a, Entity b);
}