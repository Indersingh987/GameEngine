#include "Entity.h"
#include <algorithm>

Entity::Entity(float startX, float startY, int w, int h, float spd, float grav)
    : x(startX), y(startY), width(w), height(h), speed(spd), gravity(grav) {
}

void Entity::handleInput(const Uint8* keystate) {
    moveUp    = keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP];
    moveDown  = keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_DOWN];
    moveLeft  = keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT];
    moveRight = keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT];
}

void Entity::update(float deltaTime) {
    const float groundY = 500.0f; // temporary fixed "floor" for platformer-style entities

    // Horizontal: always direct velocity from input
    velocityX = 0.0f;
    if (moveLeft)  velocityX = -speed;
    if (moveRight) velocityX = speed;

    // Vertical: behavior depends on whether this entity uses gravity
    if (gravity != 0.0f) {
        velocityY += gravity * deltaTime; // gravity constantly pulls down
    } else {
        // no gravity: vertical movement is direct, like horizontal
        velocityY = 0.0f;
        if (moveUp)   velocityY = -speed;
        if (moveDown) velocityY = speed;
    }

    x += velocityX * deltaTime;
    y += velocityY * deltaTime;

    // Simple ground collision, only meaningful for gravity-affected entities
    if (gravity != 0.0f) {
        if (y + height >= groundY) {
            y = groundY - height;
            velocityY = 0.0f;
            onGround = true;
        } else {
            onGround = false;
        }
    }
}

void Entity::jump() {
    if (gravity != 0.0f && onGround) {
        velocityY = -500.0f;
    }
}

void Entity::render(SDL_Renderer* renderer) {
    SDL_Rect rect = getBounds();
    SDL_SetRenderDrawColor(renderer, 220, 60, 60, 255);
    SDL_RenderFillRect(renderer, &rect);
}

SDL_Rect Entity::getBounds() const {
    SDL_Rect rect;
    rect.x = static_cast<int>(x);
    rect.y = static_cast<int>(y);
    rect.w = width;
    rect.h = height;
    return rect;
}

void Entity::resolveCollision(const Entity& other) {
    SDL_Rect a = getBounds();
    SDL_Rect b = other.getBounds();

    float overlapX = std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x);
    float overlapY = std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y);

    if (overlapX < overlapY) {
        if (a.x < b.x) x -= overlapX;
        else            x += overlapX;
    } else {
        if (a.y < b.y) y -= overlapY;
        else            y += overlapY;
    }
}

bool checkCollision(const Entity& a, const Entity& b) {
    SDL_Rect rectA = a.getBounds();
    SDL_Rect rectB = b.getBounds();

    return rectA.x < rectB.x + rectB.w &&
           rectA.x + rectA.w > rectB.x &&
           rectA.y < rectB.y + rectB.h &&
           rectA.y + rectA.h > rectB.y;
}