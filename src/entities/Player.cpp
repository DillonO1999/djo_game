#include "Player.hpp"
#include "Physics.hpp"
#include "raymath.h"

Player::Player() {
    position = { 490.0f, 50.0f, 490.0f };
    anims = nullptr; // Initialize to null
    animsCount = 0;
}

void Player::Init() {
    model = LoadModel("assets/characters/Brute/Brute.glb");
    anims = LoadModelAnimations("assets/characters/Brute/Brute.glb", &animsCount);
}

void Player::Update(float dt, const Model& terrain, float cameraYaw, float cameraPitch) {
    HandleInput();

    // 1. Calculate direction vectors based on camera
    Vector3 forward2D = { sinf(cameraYaw * DEG2RAD), 0, cosf(cameraYaw * DEG2RAD) };
    Vector3 right = { cosf(cameraYaw * DEG2RAD), 0, -sinf(cameraYaw * DEG2RAD) };
    
    // 2. Get environment data
    float floorY = Physics::GetHeight(position, terrain);
    Vector3 normal = Physics::GetNormal(position, terrain);

    // 3. Branch logic based on mode
    if (isCreativeMode) {
        UpdateFlying(dt, terrain, forward2D, right, cameraPitch);
    } else {
        UpdateWalking(dt, forward2D, right, floorY, normal);
    }

    // Keep player within the fence line (roughly -495 to 495)
    position.x = Clamp(position.x, -497.5f, 497.5f);
    position.z = Clamp(position.z, -497.5f, 497.5f);

    // 4. Animation
    if (anims != nullptr && animsCount > 0) {
        frameCounter++;
        UpdateModelAnimation(model, anims[currentAnim], (int)frameCounter);
        if (frameCounter >= anims[currentAnim].frameCount) frameCounter = 0;
    }

    // Smoothly transition the view height based on crouching state
    float targetHeight = isCrouching ? 0.8f : 1.5f;
    currentEyeHeight = Lerp(currentEyeHeight, targetHeight, 10.0f * dt);
}

void Player::UpdateWalking(float dt, Vector3 forward, Vector3 right, float floorY, Vector3 normal) {
    float targetMult = isSprinting ? 1.7f : (isCrouching ? 0.4f : 1.0f);
    speedMultiplier = Lerp(speedMultiplier, targetMult, 12.0f * dt);
    float currentSpeed = 7.0f * speedMultiplier;

    Vector3 moveStep = { 0, 0, 0 };
    if (IsKeyDown(KEY_W)) moveStep = Vector3Add(moveStep, forward);
    if (IsKeyDown(KEY_S)) moveStep = Vector3Subtract(moveStep, forward);
    if (IsKeyDown(KEY_A)) moveStep = Vector3Add(moveStep, right);
    if (IsKeyDown(KEY_D)) moveStep = Vector3Subtract(moveStep, right);

    if (Vector3Length(moveStep) > 0) {
        moveStep = Vector3Normalize(moveStep);
        position = Vector3Add(position, Vector3Scale(moveStep, currentSpeed * dt));
    }

    // Physics
    Physics::ApplyGravity(position, verticalVelocity, floorY, dt);
    
    if (IsKeyPressed(KEY_SPACE) && position.y <= floorY + 0.5f) {
        verticalVelocity = 8.0f;
    }
}

void Player::UpdateFlying(float dt, const Model& terrain, Vector3 forward, Vector3 right, float pitch) {
    float speed = 90.0f * dt;
    // Calculate 3D forward for flying
    Vector3 forward3D = { 
        forward.x * cosf(pitch * DEG2RAD), 
        sinf(pitch * DEG2RAD), 
        forward.z * cosf(pitch * DEG2RAD) 
    };

    if (IsKeyDown(KEY_W)) position = Vector3Add(position, Vector3Scale(forward3D, speed));
    if (IsKeyDown(KEY_S)) position = Vector3Subtract(position, Vector3Scale(forward3D, speed));
    if (IsKeyDown(KEY_A)) position = Vector3Add(position, Vector3Scale(right, speed));
    if (IsKeyDown(KEY_D)) position = Vector3Subtract(position, Vector3Scale(right, speed));
    if (IsKeyDown(KEY_SPACE)) position.y += speed;
    if (IsKeyDown(KEY_LEFT_CONTROL)) position.y -= speed;

    position.y = Clamp(position.y, Physics::GetHeight(position, terrain), 500.0f);
}

void Player::HandleInput() {
    if (IsKeyPressed(KEY_C)) isCrouching = !isCrouching;
    if (IsKeyPressed(KEY_LEFT_SHIFT)) isSprinting = !isSprinting;
    if (IsKeyPressed(KEY_G)) {
        isCreativeMode = !isCreativeMode;
        verticalVelocity = 0.0f;
    }
}

Vector3 Player::GetCameraTarget() {
    return { position.x, position.y + currentEyeHeight, position.z };
}

void Player::Draw(float cameraYaw) {
    DrawModelEx(model, position, {0, 1, 0}, cameraYaw, {1.0f, 1.0f, 1.0f}, WHITE);
}

Player::~Player() {
    for (int i = 0; i < animsCount; i++) UnloadModelAnimation(anims[i]);
    RL_FREE(anims);
    UnloadModel(model);
}