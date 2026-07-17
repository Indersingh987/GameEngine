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

    // Re-parses an entity script's file and updates its cache slot in place - see
    // ScriptManager::reloadScript's own comment for why this is safe for entity scripts (their
    // only bindings are shared globals, not per-environment) but must NOT be used for the current
    // Game/Scene script path (use setGameScriptPath/setSceneScriptPath instead, which also
    // rebind callScript/switchScene). Returns false on a missing file or parse error.
    bool reloadEntityScript(const std::string& path);

    // Pure syntax check for the script editor's Save action - see ScriptManager::checkSyntax.
    bool checkScriptSyntax(const std::string& code, const std::string& chunkName, std::string& outError, int& outErrorLine);

    // The one capability reserved for the Game tier: loads a different scene + that scene's own
    // Scene script together (bound into gameScript's env as switchScene(), never reachable from
    // Scene/Entity scripts). Validates both new paths before touching any current state - leaves
    // everything untouched and returns false if either fails to load.
    bool switchScene(const std::string& newScenePath, const std::string& newSceneScriptPath);
    const std::string& getScenePath() const;

private:
    Scene scene;
    AudioManager& audio;
    TextureManager& textures;
    ScriptManager scriptManager;

    // Game script: one per app, loaded once, persists across scene reloads. Scene script: one
    // per scene - reloaded alongside `scene` on every switchScene() call. Reassignable at
    // runtime via setGameScriptPath/setSceneScriptPath (Scripts/Scene menu popups) or switchScene.
    TierScript gameScript;
    TierScript sceneScript;
    std::string gameScriptPath;
    std::string sceneScriptPath;
    std::string scenePath = "assets/scene.json";

    bool playing = false;
    nlohmann::json playSnapshot;
    std::string playSnapshotScenePath;
    std::string playSnapshotSceneScriptPath;

   void createPhysicsBody(Entity entity);
};