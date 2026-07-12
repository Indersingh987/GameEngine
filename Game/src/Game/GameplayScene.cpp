#include "Game/GameplayScene.h"

GameplayScene::GameplayScene(AudioManager& audioRef)
    : audio(audioRef) {

    b2WorldId world = scene.getPhysicsWorld();

    // --- Player: dynamic body ---
    player = scene.createEntity();
    scene.addComponent<TransformComponent>(player, {350.0f, 250.0f, 100, 100});
    scene.addComponent<SpriteComponent>(player, {220, 60, 60, 255});

    b2BodyDef playerBodyDef = b2DefaultBodyDef();
    playerBodyDef.type = b2_dynamicBody;
    playerBodyDef.position.x = 350.0f / PIXELS_PER_METER;
    playerBodyDef.position.y = 250.0f / PIXELS_PER_METER;
    playerBodyDef.gravityScale = 0.0f; // start with no gravity, space-style

    b2BodyId playerBodyId = b2CreateBody(world, &playerBodyDef);

    b2Polygon playerBox = b2MakeBox(
        (100.0f / PIXELS_PER_METER) / 2.0f,
        (100.0f / PIXELS_PER_METER) / 2.0f
    );
    b2ShapeDef playerShapeDef = b2DefaultShapeDef();
    playerShapeDef.density = 1.0f;
    playerShapeDef.material.friction = 0.3f;
    b2CreatePolygonShape(playerBodyId, &playerShapeDef, &playerBox);

    scene.addComponent<PhysicsComponent>(player, {playerBodyId});

    // --- Obstacle: static body ---
    obstacle = scene.createEntity();
    scene.addComponent<TransformComponent>(obstacle, {100.0f, 100.0f, 80, 80});
    scene.addComponent<SpriteComponent>(obstacle, {220, 60, 60, 255});

    b2BodyDef obstacleBodyDef = b2DefaultBodyDef();
    obstacleBodyDef.type = b2_staticBody;
    obstacleBodyDef.position.x = 100.0f / PIXELS_PER_METER;
    obstacleBodyDef.position.y = 100.0f / PIXELS_PER_METER;

    b2BodyId obstacleBodyId = b2CreateBody(world, &obstacleBodyDef);

    b2Polygon obstacleBox = b2MakeBox(
        (80.0f / PIXELS_PER_METER) / 2.0f,
        (80.0f / PIXELS_PER_METER) / 2.0f
    );
    b2ShapeDef obstacleShapeDef = b2DefaultShapeDef();
    b2CreatePolygonShape(obstacleBodyId, &obstacleShapeDef, &obstacleBox);

    scene.addComponent<PhysicsComponent>(obstacle, {obstacleBodyId});
}

void GameplayScene::handleInput(const Uint8* keystate) {
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
    b2World_Step(scene.getPhysicsWorld(), deltaTime, 4);

    Systems::physics(scene, player, deltaTime);
    Systems::physics(scene, obstacle, deltaTime);
}

void GameplayScene::render(SDL_Renderer* renderer) {
    Systems::render(scene, player, renderer);
    Systems::render(scene, obstacle, renderer);
}