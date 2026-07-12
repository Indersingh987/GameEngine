#pragma once

#include "Scene.h"
#include "Entity.h"
#include "AudioManager.h"

class GameplayScene : public Scene {
public:
    GameplayScene(AudioManager& audio);

    void handleInput(const Uint8* keystate) override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer) override;

private:
    Entity player;
    Entity obstacle;
    AudioManager& audio;
};