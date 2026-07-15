#pragma once

#include "Engine/ECS/Scene.h"
#include "Engine/ECS/Systems.h"
#include "Engine/AudioManager.h"
#include "Engine/TextureManager.h"
#include "Engine/ScriptManager.h"
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
    Entity createEntity(const std::string& displayName, BodyType bodyType, bool isPlayer,
                         const std::string& scriptPath = "");

    void play();
    void stop();
    bool isPlaying() const;

private:
    Scene scene;
    AudioManager& audio;
    TextureManager& textures;
    ScriptManager scriptManager;

    bool playing = false;
    nlohmann::json playSnapshot;

   void createPhysicsBody(Entity entity);
};