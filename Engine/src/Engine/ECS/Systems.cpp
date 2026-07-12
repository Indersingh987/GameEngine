#include "Engine/ECS/Systems.h"

namespace Systems {

void physics(Scene& scene, Entity entity, float deltaTime) {
    if (!scene.hasComponent<PhysicsComponent>(entity) || !scene.hasComponent<TransformComponent>(entity)) {
        return;
    }

    auto& phys = scene.getComponent<PhysicsComponent>(entity);
    auto& transform = scene.getComponent<TransformComponent>(entity);

    if (B2_IS_NULL(phys.bodyId)) {
        return;
    }

    b2Vec2 position = b2Body_GetPosition(phys.bodyId);
    transform.x = (position.x * PIXELS_PER_METER) - (transform.width / 2.0f);
    transform.y = (position.y * PIXELS_PER_METER) - (transform.height / 2.0f);
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

}