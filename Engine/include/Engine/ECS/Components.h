#pragma once

#include <box2d/box2d.h>
#include <SDL2/SDL.h>

constexpr float PIXELS_PER_METER = 50.0f;

struct TransformComponent {
    float x = 0.0f;
    float y = 0.0f;
    int width = 0;
    int height = 0;
};

struct PhysicsComponent {
    b2BodyId bodyId = b2_nullBodyId;
};

struct SpriteComponent {
    Uint8 r = 255;
    Uint8 g = 255;
    Uint8 b = 255;
    Uint8 a = 255;
};