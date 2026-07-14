#pragma once

#include "Engine/ECS/Scene.h"
#include "Engine/ECS/Systems.h"
#include "Engine/AudioManager.h"
#include "Engine/TextureManager.h"
#include <nlohmann/json.hpp>

class GameplayScene {
public:
    GameplayScene(AudioManager& audio, TextureManager& textures);

    void handleInput(const Uint8* keystate);
    void update(float deltaTime);
    void render(SDL_Renderer* renderer);
    Scene& getScene();
    void reinitializePhysics();
    void reinitializeTextures();
    Entity createDefaultEntity();

    void play();
    void stop();
    bool isPlaying() const;

private:
    Scene scene;
    AudioManager& audio;
    TextureManager& textures;

    bool playing = false;
    nlohmann::json playSnapshot;

   void createPhysicsBody(Entity entity);
};