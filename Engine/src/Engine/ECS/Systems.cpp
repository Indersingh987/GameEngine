#include "Engine/ECS/Systems.h"
#include <iostream>

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

void script(Scene& scene, Entity entity, float deltaTime, ScriptManager& scriptManager) {
    if (!scene.hasComponent<ScriptComponent>(entity)) {
        return;
    }

    auto& scriptComp = scene.getComponent<ScriptComponent>(entity);
    if (scriptComp.scriptPath.empty()) {
        return;
    }

    ScriptData* data = scriptManager.loadScript(scriptComp.scriptPath);
    if (data == nullptr || !data->onUpdate.valid()) {
        return;
    }

    sol::protected_function_result result = data->onUpdate(entity, deltaTime);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Script error in '" << scriptComp.scriptPath << "' onUpdate: " << err.what() << std::endl;
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

    if (sprite.texture != nullptr) {
        SDL_RenderCopy(renderer, sprite.texture, nullptr, &rect);
    } else {
        SDL_SetRenderDrawColor(renderer, sprite.r, sprite.g, sprite.b, sprite.a);
        SDL_RenderFillRect(renderer, &rect);
    }
}

}