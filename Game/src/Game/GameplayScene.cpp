#include "Game/GameplayScene.h"

GameplayScene::GameplayScene(AudioManager& audioRef)
    : player(350.0f, 250.0f, 100, 100, 300.0f, 0.0f),
      obstacle(100.0f, 100.0f, 80, 80, 0.0f, 0.0f),
      audio(audioRef) {
}

void GameplayScene::handleInput(const Uint8* keystate) {
    player.handleInput(keystate);
}

void GameplayScene::update(float deltaTime) {
    player.update(deltaTime);

    if (checkCollision(player, obstacle)) {
        player.resolveCollision(obstacle);
    }
}

void GameplayScene::render(SDL_Renderer* renderer) {
    player.render(renderer);
    obstacle.render(renderer);
}