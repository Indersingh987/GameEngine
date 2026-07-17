#include "Game/GameplayScene.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
constexpr const char* GAME_SCRIPT_PATH = "assets/scripts/Game/main_game.lua";
constexpr const char* SCENE_SCRIPT_PATH = "assets/scripts/Scenes/main_scene.lua";
}

GameplayScene::GameplayScene(AudioManager& audioRef, TextureManager& texturesRef)
    : audio(audioRef), textures(texturesRef), scriptManager(scene, audio) {

    gameScript.load(GAME_SCRIPT_PATH, scriptManager);
    sceneScript.load(SCENE_SCRIPT_PATH, scriptManager);
    gameScriptPath = GAME_SCRIPT_PATH;
    sceneScriptPath = SCENE_SCRIPT_PATH;
    sceneScript.bindSceneCallScript(gameScript, scriptManager);
    gameScript.bindCallScript("scene", sceneScript);
    gameScript.bindSwitchScene([this](const std::string& newScenePath, const std::string& newSceneScriptPath) {
        return switchScene(newScenePath, newSceneScriptPath);
    });
    scriptManager.setSceneScript(&sceneScript);

    std::ifstream saveFile("assets/scene.json");
    if (saveFile.good()) {
        scene.loadFromFile("assets/scene.json");
        reinitializePhysics();
        reinitializeTextures();
        return;
    }

    Entity player = scene.createEntity();
    scene.addComponent<TransformComponent>(player, {350.0f, 250.0f, 100, 100});
    scene.addComponent<SpriteComponent>(player, {220, 60, 60, 255});
    scene.addComponent<PhysicsComponent>(player, {b2_nullBodyId, BodyType::Dynamic, 0.0f});
    scene.addComponent<TagComponent>(player, {"Player", "player"});
    createPhysicsBody(player);

    Entity obstacle = scene.createEntity();
    scene.addComponent<TransformComponent>(obstacle, {100.0f, 100.0f, 80, 80});
    scene.addComponent<SpriteComponent>(obstacle, {220, 60, 60, 255});
    scene.addComponent<PhysicsComponent>(obstacle, {b2_nullBodyId, BodyType::Static, 0.0f});
    scene.addComponent<TagComponent>(obstacle, {"Obstacle", ""});
    createPhysicsBody(obstacle);
}

void GameplayScene::handleInput(const Uint8* keystate) {
    if (!playing) {
        return;
    }

    Entity player = scene.findEntityByRole("player");
    if (player == INVALID_ENTITY) {
        return;
    }

    // A scripted player owns its own velocity via onUpdate (e.g. flap.lua) - this legacy
    // WASD/arrow control predates scripting and would otherwise fight it for the same body.
    if (scene.hasComponent<ScriptComponent>(player)) {
        return;
    }

    auto& phys = scene.getComponent<PhysicsComponent>(player);

    float vx = 0.0f;
    float vy = 0.0f;
    float speed = 6.0f; // meters per second now, not pixels

    if (keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT])  vx = -speed;
    if (keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT]) vx = speed;
    if (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP])    vy = -speed;
    if (keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_DOWN])  vy = speed;

    b2Vec2 velocity = {vx, vy};
    b2Body_SetLinearVelocity(phys.bodyId, velocity);
}

void GameplayScene::update(float deltaTime) {
    if (!playing) {
        return;
    }

    b2World_Step(scene.getPhysicsWorld(), deltaTime, 4);

    for (Entity entity : scene.getAllEntities()) {
        Systems::physics(scene, entity, deltaTime);
    }

    // Snapshot the entity list before running scripts - onUpdate is allowed to call
    // spawnEntity/destroyEntity, which mutate Scene's underlying vector. Iterating the
    // live reference (as the physics loop above does safely, since it never mutates)
    // would invalidate this loop mid-iteration.
    std::vector<Entity> entitiesForScripts = scene.getAllEntities();
    for (Entity entity : entitiesForScripts) {
        Systems::script(scene, entity, deltaTime, scriptManager);
    }
}

void GameplayScene::render(SDL_Renderer* renderer) {
    for (Entity entity : scene.getAllEntities()) {
        Systems::render(scene, entity, renderer);
    }
}

Scene& GameplayScene::getScene() {
    return scene;
}

Entity GameplayScene::createEntity(const std::string& displayName, BodyType bodyType, bool isPlayer,
                                    const std::string& scriptPath) {
    if (isPlayer) {
        Entity previousPlayer = scene.findEntityByRole("player");
        if (previousPlayer != INVALID_ENTITY) {
            scene.getComponent<TagComponent>(previousPlayer).role = "";
        }
    }

    // Spawn at a fixed default position (center of the 800x600 window), not the mouse cursor -
    // the user drags it into place afterward.
    constexpr int defaultWidth = 50;
    constexpr int defaultHeight = 50;
    constexpr float windowCenterX = 400.0f;
    constexpr float windowCenterY = 300.0f;

    Entity entity = scene.createEntity();
    scene.addComponent<TransformComponent>(entity, {
        windowCenterX - defaultWidth / 2.0f,
        windowCenterY - defaultHeight / 2.0f,
        defaultWidth, defaultHeight
    });
    scene.addComponent<SpriteComponent>(entity, {200, 200, 200, 255});
    scene.addComponent<PhysicsComponent>(entity, {b2_nullBodyId, bodyType, 0.0f});
    scene.addComponent<TagComponent>(entity, {displayName, isPlayer ? "player" : ""});
    if (!scriptPath.empty()) {
        scene.addComponent<ScriptComponent>(entity, {scriptPath});
    }
    createPhysicsBody(entity);
    return entity;
}

void GameplayScene::createPhysicsBody(Entity entity) {
    scene.createPhysicsBody(entity);
}

void GameplayScene::reinitializePhysics() {
    for (Entity entity : scene.getAllEntities()) {
        if (scene.hasComponent<PhysicsComponent>(entity)) {
            createPhysicsBody(entity);
        }
    }
}

void GameplayScene::reinitializeTextures() {
    for (Entity entity : scene.getAllEntities()) {
        if (scene.hasComponent<SpriteComponent>(entity)) {
            auto& sprite = scene.getComponent<SpriteComponent>(entity);
            if (!sprite.texturePath.empty()) {
                sprite.texture = textures.loadTexture(sprite.texturePath, sprite.texturePath);
            }
        }
    }
}

void GameplayScene::play() {
    if (playing) {
        return;
    }
    playSnapshot = scene.captureSnapshot();
    playSnapshotScenePath = scenePath;
    playSnapshotSceneScriptPath = sceneScriptPath;
    playing = true;
}

void GameplayScene::stop() {
    if (!playing) {
        return;
    }
    scene.restoreSnapshot(playSnapshot);
    reinitializePhysics();
    reinitializeTextures();
    // A switchScene() call during Play is transient gameplay state, same as any other
    // script-driven mutation - Stop reverts to exactly the scene (and scene script) that was
    // active when Play was pressed, not wherever the game wandered to.
    if (sceneScriptPath != playSnapshotSceneScriptPath) {
        setSceneScriptPath(playSnapshotSceneScriptPath);
    }
    scenePath = playSnapshotScenePath;
    playing = false;
}

bool GameplayScene::isPlaying() const {
    return playing;
}

TierScript& GameplayScene::getGameScript() {
    return gameScript;
}

TierScript& GameplayScene::getSceneScript() {
    return sceneScript;
}

const std::string& GameplayScene::getGameScriptPath() const {
    return gameScriptPath;
}

const std::string& GameplayScene::getSceneScriptPath() const {
    return sceneScriptPath;
}

bool GameplayScene::setGameScriptPath(const std::string& path) {
    TierScript newGameScript;
    if (!newGameScript.load(path, scriptManager)) {
        return false;
    }
    // sceneScript's existing callScript("game", ...) binding captured gameScript by reference,
    // not by value - reassigning gameScript's contents (not its address) is all that's needed
    // for that binding to pick up the new script automatically. Only the NEW script's own
    // environment needs a fresh callScript bound into it.
    gameScript = newGameScript;
    gameScriptPath = path;
    gameScript.bindCallScript("scene", sceneScript);
    gameScript.bindSwitchScene([this](const std::string& newScenePath, const std::string& newSceneScriptPath) {
        return switchScene(newScenePath, newSceneScriptPath);
    });
    return true;
}

bool GameplayScene::setSceneScriptPath(const std::string& path) {
    TierScript newSceneScript;
    if (!newSceneScript.load(path, scriptManager)) {
        return false;
    }
    sceneScript = newSceneScript;
    sceneScriptPath = path;
    sceneScript.bindSceneCallScript(gameScript, scriptManager);
    scriptManager.setSceneScript(&sceneScript);
    return true;
}

const std::string& GameplayScene::getScenePath() const {
    return scenePath;
}

bool GameplayScene::switchScene(const std::string& newScenePath, const std::string& newSceneScriptPath) {
    // Validate both new paths before mutating anything - same all-or-nothing contract as
    // setGameScriptPath/setSceneScriptPath. Scene script first (cheap, no side effects yet).
    TierScript newSceneScript;
    if (!newSceneScript.load(newSceneScriptPath, scriptManager)) {
        std::cerr << "switchScene: failed to load scene script: " << newSceneScriptPath << std::endl;
        return false;
    }
    // Scene::loadFromFile doesn't guard a missing file itself (unlike ScriptManager::loadScript -
    // see Known Gotcha #14) - this path is reachable from Lua, so check explicitly rather than
    // risking an unguarded stream-read failure.
    if (!std::filesystem::exists(newScenePath)) {
        std::cerr << "switchScene: scene file not found: " << newScenePath << std::endl;
        return false;
    }

    scene.loadFromFile(newScenePath);
    reinitializePhysics();
    reinitializeTextures();

    sceneScript = newSceneScript;
    sceneScriptPath = newSceneScriptPath;
    sceneScript.bindSceneCallScript(gameScript, scriptManager);
    scriptManager.setSceneScript(&sceneScript);

    scenePath = newScenePath;
    return true;
}