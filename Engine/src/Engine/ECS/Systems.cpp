#include "Engine/ECS/Systems.h"
#include <algorithm>

namespace Systems {

void movement(Scene& scene, Entity entity, float deltaTime) {
    if (!scene.hasComponent<TransformComponent>(entity) || !scene.hasComponent<VelocityComponent>(entity)) {
        return;
    }

    auto& transform = scene.getComponent<TransformComponent>(entity);
    auto& velocity = scene.getComponent<VelocityComponent>(entity);

    transform.x += velocity.vx * deltaTime;
    transform.y += velocity.vy * deltaTime;
}

void physics(Scene& scene, Entity entity, float deltaTime) {
    if (!scene.hasComponent<PhysicsComponent>(entity) || !scene.hasComponent<VelocityComponent>(entity)) {
        return;
    }

    auto& phys = scene.getComponent<PhysicsComponent>(entity);
    auto& velocity = scene.getComponent<VelocityComponent>(entity);

    if (phys.gravity != 0.0f) {
        velocity.vy += phys.gravity * deltaTime;
    }
}

void render(Scene& scene, Entity entity, SDL_Renderer* renderer) {
    if (!scene.hasComponent<TransformComponent>(entity) || !scene.hasComponent<SpriteComponent>(entity)) {
        return;
    }

    auto& transform = scene.getComponent<TransformComponent>(entity);
    auto& sprite = scene.getComponent<SpriteComponent>(entity);

    SDL_Rect rect;
    rect.x = static_cast<int>(transform.x);
    rect.y = static_cast<int>(transform.y);
    rect.w = transform.width;
    rect.h = transform.height;

    SDL_SetRenderDrawColor(renderer, sprite.r, sprite.g, sprite.b, sprite.a);
    SDL_RenderFillRect(renderer, &rect);
}

bool checkCollision(Scene& scene, Entity a, Entity b) {
    auto& tA = scene.getComponent<TransformComponent>(a);
    auto& tB = scene.getComponent<TransformComponent>(b);

    return tA.x < tB.x + tB.width &&
           tA.x + tA.width > tB.x &&
           tA.y < tB.y + tB.height &&
           tA.y + tA.height > tB.y;
}

void resolveCollision(Scene& scene, Entity a, Entity b) {
    auto& tA = scene.getComponent<TransformComponent>(a);
    auto& tB = scene.getComponent<TransformComponent>(b);

    float overlapX = std::min(tA.x + tA.width, tB.x + tB.width) - std::max(tA.x, tB.x);
    float overlapY = std::min(tA.y + tA.height, tB.y + tB.height) - std::max(tA.y, tB.y);

    if (overlapX < overlapY) {
        if (tA.x < tB.x) tA.x -= overlapX;
        else              tA.x += overlapX;
    } else {
        if (tA.y < tB.y) tA.y -= overlapY;
        else              tA.y += overlapY;
    }
}

}