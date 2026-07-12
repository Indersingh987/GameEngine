#pragma once

#include <SDL2/SDL.h>

class Scene {
public:
    virtual ~Scene() {}

    virtual void handleInput(const Uint8* keystate) = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render(SDL_Renderer* renderer) = 0;
};