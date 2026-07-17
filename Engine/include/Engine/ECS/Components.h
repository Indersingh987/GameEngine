#pragma once

#include <box2d/box2d.h>
#include <SDL2/SDL.h>
#include <string>

constexpr float PIXELS_PER_METER = 50.0f;

struct TransformComponent {
    float x = 0.0f;
    float y = 0.0f;
    int width = 0;
    int height = 0;
};

enum class BodyType {
    Static,
    Kinematic,
    Dynamic
};

struct PhysicsComponent {
    b2BodyId bodyId = b2_nullBodyId;
    BodyType bodyType = BodyType::Dynamic;
    float gravityScale = 0.0f;
};

struct SpriteComponent {
    Uint8 r = 255;
    Uint8 g = 255;
    Uint8 b = 255;
    Uint8 a = 255;

    std::string texturePath;       // empty = use flat color, non-empty = use this image
    SDL_Texture* texture = nullptr; // runtime handle, not serialized - reloaded from texturePath
};

struct TagComponent {
    std::string displayName;  // cosmetic, shown in Scene Hierarchy (e.g. "Bird", "Pipe_Top_3")
    std::string role;         // functional (e.g. "player"), drives behavior lookups. Empty = no role.
};

struct ScriptComponent {
    std::string scriptPath;  // e.g. "assets/scripts/Entities/flap.lua". Runtime handle added in a later step.
};