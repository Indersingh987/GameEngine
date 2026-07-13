#pragma once

#include "Engine/ECS/Scene.h"
#include "Engine/ECS/Systems.h"
#include "Engine/AudioManager.h"

class GameplayScene {
public:
    GameplayScene(AudioManager& audio);

    void handleInput(const Uint8* keystate);
    void update(float deltaTime);
    void render(SDL_Renderer* renderer);
    Scene& getScene();
    void reinitializePhysics();
    Entity createDefaultEntity();

private:
    Scene scene;
    Entity player;
    Entity obstacle;
    AudioManager& audio;

   void createPhysicsBody(Entity entity);
};