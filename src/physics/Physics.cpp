#include "Physics.hpp"
#include "raymath.h"

namespace Physics {

    float GetHeight(Vector3 pos, const Model& terrain) {
        float highestPoint = -100.0f; // Default "void" height
        Ray ray = { { pos.x, pos.y + 50.0f, pos.z }, { 0, -1, 0 } };

        // Loop through EVERY mesh in the model to find the ground
        for (int i = 0; i < terrain.meshCount; i++) {
            RayCollision hit = GetRayCollisionMesh(ray, terrain.meshes[i], terrain.transform);
            if (hit.hit && hit.point.y > highestPoint) {
                highestPoint = hit.point.y;
            }
        }
        return (highestPoint == -100.0f) ? 0.0f : highestPoint;
    }

    void UpdateBall(Ball& ball, const Model& terrain, float dt) {
        // 1. Gravity and Movement
        ball.velocity.y -= 15.0f * dt; 
        ball.position = Vector3Add(ball.position, Vector3Scale(ball.velocity, dt));

        float floorY = GetHeight(ball.position, terrain);
        
        // 2. Collision Resolution
        if (ball.position.y - ball.radius < floorY) {
            ball.position.y = floorY + ball.radius; 
            
            Vector3 normal = GetNormal(ball.position, terrain);

            if (ball.velocity.y < 0) {
                ball.velocity = Vector3Reflect(ball.velocity, normal);
                ball.velocity = Vector3Scale(ball.velocity, ball.restitution);
                
                // Transition to smooth slide if bounce is low
                if (ball.velocity.y < 1.0f) {
                    float dot = Vector3DotProduct(ball.velocity, normal);
                    ball.velocity = Vector3Subtract(ball.velocity, Vector3Scale(normal, dot));
                }
            }

            // 3. Sliding Damping
            // Use 0.995f for a very slick, long-lasting slide
            float friction = 0.995f; 

            // Only apply friction if the ball is actually on the ground
            if (ball.velocity.y < 0.1f) {
                ball.velocity.x *= friction;
                ball.velocity.z *= friction;
            }

            // 4. Slope Acceleration (Make this stronger to help it slide)
            float rollInfluence = 25.0f; 
            ball.velocity.x += normal.x * rollInfluence * dt;
            ball.velocity.z += normal.z * rollInfluence * dt;

            if (Vector3Length(ball.velocity) < 0.05f) ball.velocity = { 0, 0, 0 };
        } // <--- This bracket closes the IF collision

        // 4. Boundary Clamping (Must be INSIDE UpdateBall function)
        ball.position.x = Clamp(ball.position.x, -497.5f + ball.radius, 497.5f - ball.radius);
        ball.position.z = Clamp(ball.position.z, -497.5f + ball.radius, 497.5f - ball.radius);
    } // <--- This bracket closes UpdateBall

    Vector3 GetNormal(Vector3 pos, const Model& terrain) {
        Ray ray = { { pos.x, pos.y + 50.0f, pos.z }, { 0, -1, 0 } };
        float highestPoint = -100.0f;
        Vector3 bestNormal = { 0, 1, 0 };

        for (int i = 0; i < terrain.meshCount; i++) {
            RayCollision hit = GetRayCollisionMesh(ray, terrain.meshes[i], terrain.transform);
            if (hit.hit && hit.point.y > highestPoint) {
                highestPoint = hit.point.y;
                bestNormal = hit.normal;
            }
        }
        
        return bestNormal;
    }

    void ApplyGravity(Vector3& pos, float& verticalVel, float floorY, float dt) {
        // 1. Apply downward force
        verticalVel -= GRAVITY * dt;
        pos.y += verticalVel * dt;

        // 2. Ground Snap
        if (pos.y <= floorY) {
            pos.y = floorY;
            verticalVel = 0;
        }
    }
}