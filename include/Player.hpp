#pragma once
#include "raylib.h"

class Player {
    public:
        Player();
        ~Player();

        // In Player.hpp
        void Init();
        
        // This is the main entry point called by Game.cpp
        void Update(float dt, const Model& terrain, float cameraYaw, float cameraPitch);
        void Draw(float cameraYaw);

        Vector3 GetCameraTarget(); 
        Vector3 position;
        bool isCreativeMode = false;

    private:
        // Internal state helpers
        void HandleInput();
        void UpdateWalking(float dt, Vector3 forward, Vector3 right, float floorY, Vector3 normal);
        void UpdateFlying(float dt, const Model& terrain, Vector3 forward, Vector3 right, float cameraPitch);

        // Assets
        Model model;
        ModelAnimation* anims;
        int currentAnim = 0;
        float frameCounter = 0;
        int animsCount = 0;

        // Movement state
        float verticalVelocity = 0.0f;
        float speedMultiplier = 1.0f;
        bool isGrounded = false;
        bool isSprinting = false;
        bool isCrouching = false;
        float currentEyeHeight = 1.5f;
};