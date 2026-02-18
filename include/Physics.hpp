#pragma once
#include "raylib.h"

// 1. Define the Ball struct FIRST
struct Ball {
    Vector3 position;
    Vector3 velocity;
    float radius;
    float restitution;
};

namespace Physics {
    const float GRAVITY = 18.0f;
    const float TERMINAL_VELOCITY = 50.0f;

    float GetHeight(Vector3 pos, const Model& terrain);
    Vector3 GetNormal(Vector3 pos, const Model& terrain);
    void UpdateBall(Ball& ball, const Model& terrain, float dt);
    
    // Updated to include floorY so gravity knows where the ground is
    void ApplyGravity(Vector3& pos, float& vel, float floorY, float dt);
}