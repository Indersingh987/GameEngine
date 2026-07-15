#include "Game/GameplayScene.h"
#include <fstream>

GameplayScene::GameplayScene(AudioManager& audioRef, TextureManager& texturesRef)
    : audio(audioRef), textures(texturesRef), scriptManager(scene, audio) {

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
    playing = true;
}

void GameplayScene::stop() {
    if (!playing) {
        return;
    }
    scene.restoreSnapshot(playSnapshot);
    reinitializePhysics();
    reinitializeTextures();
    playing = false;
}

bool GameplayScene::isPlaying() const {
    return playing;
}