#include "Game/GameplayScene.h"

GameplayScene::GameplayScene(AudioManager& audioRef)
    : audio(audioRef) {

    player = scene.createEntity();
    scene.addComponent<TransformComponent>(player, {350.0f, 250.0f, 100, 100});
    scene.addComponent<SpriteComponent>(player, {220, 60, 60, 255});
    scene.addComponent<PhysicsComponent>(player, {b2_nullBodyId, true, 0.0f});
    createPhysicsBody(player);

    obstacle = scene.createEntity();
    scene.addComponent<TransformComponent>(obstacle, {100.0f, 100.0f, 80, 80});
    scene.addComponent<SpriteComponent>(obstacle, {220, 60, 60, 255});
    scene.addComponent<PhysicsComponent>(obstacle, {b2_nullBodyId, false, 0.0f});
    createPhysicsBody(obstacle);
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

Scene& GameplayScene::getScene() {
    return scene;
}

void GameplayScene::createPhysicsBody(Entity entity) {
    auto& transform = scene.getComponent<TransformComponent>(entity);
    auto& phys = scene.getComponent<PhysicsComponent>(entity);
    b2WorldId world = scene.getPhysicsWorld();

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = phys.isDynamic ? b2_dynamicBody : b2_staticBody;
    bodyDef.position.x = (transform.x + transform.width / 2.0f) / PIXELS_PER_METER;
    bodyDef.position.y = (transform.y + transform.height / 2.0f) / PIXELS_PER_METER;
    bodyDef.gravityScale = phys.gravityScale;

    b2BodyId bodyId = b2CreateBody(world, &bodyDef);

    b2Polygon box = b2MakeBox(
        (transform.width / PIXELS_PER_METER) / 2.0f,
        (transform.height / PIXELS_PER_METER) / 2.0f
    );
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.0f;
    shapeDef.material.friction = 0.3f;
    b2CreatePolygonShape(bodyId, &shapeDef, &box);

    phys.bodyId = bodyId;
}

void GameplayScene::reinitializePhysics() {
    createPhysicsBody(player);
    createPhysicsBody(obstacle);
}