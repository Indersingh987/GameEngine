#include "Game/GameplayScene.h"

GameplayScene::GameplayScene(AudioManager& audioRef)
    : audio(audioRef) {

    // --- Create player entity ---
    player = scene.createEntity();
    scene.addComponent<TransformComponent>(player, {350.0f, 250.0f, 100, 100});
    scene.addComponent<VelocityComponent>(player, {0.0f, 0.0f});
    scene.addComponent<PhysicsComponent>(player, {0.0f, false}); // gravity = 0
    scene.addComponent<SpriteComponent>(player, {220, 60, 60, 255}); // red

    // --- Create obstacle entity ---
    obstacle = scene.createEntity();
    scene.addComponent<TransformComponent>(obstacle, {100.0f, 100.0f, 80, 80});
    scene.addComponent<SpriteComponent>(obstacle, {220, 60, 60, 255}); // red
    // no VelocityComponent/PhysicsComponent — obstacle is static
}

void GameplayScene::handleInput(const Uint8* keystate) {
    auto& velocity = scene.getComponent<VelocityComponent>(player);
    auto& phys = scene.getComponent<PhysicsComponent>(player);

    velocity.vx = 0.0f;
    if (keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT])  velocity.vx = -300.0f;
    if (keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT]) velocity.vx = 300.0f;

    if (phys.gravity == 0.0f) {
        velocity.vy = 0.0f;
        if (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP])   velocity.vy = -300.0f;
        if (keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_DOWN]) velocity.vy = 300.0f;
    }
}

void GameplayScene::update(float deltaTime) {
    Systems::physics(scene, player, deltaTime);
    Systems::movement(scene, player, deltaTime);

    if (Systems::checkCollision(scene, player, obstacle)) {
        Systems::resolveCollision(scene, player, obstacle);
    }
}

void GameplayScene::render(SDL_Renderer* renderer) {
    Systems::render(scene, player, renderer);
    Systems::render(scene, obstacle, renderer);
}