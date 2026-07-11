#pragma once

#include <SDL2/SDL.h>

class Entity {
public:
    Entity(float x, float y, int width, int height, float speed, float gravity = 0.0f);

    void handleInput(const Uint8* keystate);
    void update(float deltaTime);
    void render(SDL_Renderer* renderer);
    void jump();
    void resolveCollision(const Entity& other);

    SDL_Rect getBounds() const;

private:
    float x, y;
    int width, height;
    float speed;

    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float gravity;      // 0 = no gravity (top-down/space style), >0 = platformer style
    bool onGround = false;

    bool moveUp = false;
    bool moveDown = false;
    bool moveLeft = false;
    bool moveRight = false;
};

bool checkCollision(const Entity& a, const Entity& b);