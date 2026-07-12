#pragma once
#include <SDL2/SDL.h>

struct TransformComponent {
    float x = 0.0f;
    float y = 0.0f;
    int width = 0;
    int height = 0;
};

struct VelocityComponent {
    float vx = 0.0f;
    float vy = 0.0f;
};

struct PhysicsComponent {
    float gravity = 0.0f;
    bool onGround = false;
};

struct SpriteComponent {
    Uint8 r = 255;
    Uint8 g = 255;
    Uint8 b = 255;
    Uint8 a = 255;
};