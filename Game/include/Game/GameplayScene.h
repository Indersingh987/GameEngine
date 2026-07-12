#pragma once

#include "Engine/Scene.h"
#include "Engine/Entity.h"
#include "Engine/AudioManager.h"

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