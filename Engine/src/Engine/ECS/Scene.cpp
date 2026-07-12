#include "Engine/ECS/Scene.h"

Entity Scene::createEntity() {
    return nextEntityId++;
}

void Scene::destroyEntity(Entity entity) {
    transforms.erase(entity);
    velocities.erase(entity);
    physics.erase(entity);
    sprites.erase(entity);
}

// Template specializations — tell the compiler which storage map corresponds to which component type
template<>
std::unordered_map<Entity, TransformComponent>& Scene::getStorage<TransformComponent>() {
    return transforms;
}

template<>
std::unordered_map<Entity, VelocityComponent>& Scene::getStorage<VelocityComponent>() {
    return velocities;
}

template<>
std::unordered_map<Entity, PhysicsComponent>& Scene::getStorage<PhysicsComponent>() {
    return physics;
}

template<>
std::unordered_map<Entity, SpriteComponent>& Scene::getStorage<SpriteComponent>() {
    return sprites;
}