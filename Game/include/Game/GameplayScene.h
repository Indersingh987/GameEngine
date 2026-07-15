#pragma once

#include "Engine/ECS/Scene.h"
#include "Engine/ECS/Systems.h"
#include "Engine/AudioManager.h"
#include "Engine/TextureManager.h"
#include "Engine/ScriptManager.h"
#include "Engine/TierScript.h"
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

    TierScript& getGameScript();
    TierScript& getSceneScript();

    const std::string& getGameScriptPath() const;
    const std::string& getSceneScriptPath() const;

    // Loads the script at path and, only on success, replaces the current Game/Scene script and
    // rebinds its callScript. Returns false (leaving the current script untouched) if the file
    // fails to load/parse - callers should surface that to the user rather than silently no-op.
    bool setGameScriptPath(const std::string& path);
    bool setSceneScriptPath(const std::string& path);

private:
    Scene scene;
    AudioManager& audio;
    TextureManager& textures;
    ScriptManager scriptManager;

    // Game script: one per app, loaded once, persists across scene reloads. Scene script: one
    // per scene - reloaded alongside `scene` once scene switching exists. Reassignable at
    // runtime via setGameScriptPath/setSceneScriptPath (Scripts/Scene menu popups).
    TierScript gameScript;
    TierScript sceneScript;
    std::string gameScriptPath;
    std::string sceneScriptPath;

    bool playing = false;
    nlohmann::json playSnapshot;

   void createPhysicsBody(Entity entity);
};