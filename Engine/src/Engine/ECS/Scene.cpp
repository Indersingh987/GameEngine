#include "Engine/ECS/Scene.h"

Scene::Scene() {
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity.x = 0.0f;
    worldDef.gravity.y = 9.8f;
    physicsWorldId = b2CreateWorld(&worldDef);
}

Scene::~Scene() {
    b2DestroyWorld(physicsWorldId);
}

b2WorldId Scene::getPhysicsWorld() {
    return physicsWorldId;
}
Entity Scene::createEntity() {
    return nextEntityId++;
}

void Scene::destroyEntity(Entity entity) {
    transforms.erase(entity);
    physics.erase(entity);
    sprites.erase(entity);
}

// Template specializations — tell the compiler which storage map corresponds to which component type
template<>
std::unordered_map<Entity, TransformComponent>& Scene::getStorage<TransformComponent>() {
    return transforms;
}

template<>
std::unordered_map<Entity, PhysicsComponent>& Scene::getStorage<PhysicsComponent>() {
    return physics;
}

template<>
std::unordered_map<Entity, SpriteComponent>& Scene::getStorage<SpriteComponent>() {
    return sprites;
}