#include "Game.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <algorithm>

Game::Game() {
    SetConfigFlags(FLAG_FULLSCREEN_MODE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(0, 0, "Real 3D - Raylib Version");
    SetTargetFPS(60);
    DisableCursor();
    SetExitKey(KEY_NULL);

    playerPos = (Vector3){ 490.0f, 50.0f, 490.0f }; // Start high up
    cameraYaw = 225.0f;
    cameraPitch = -20.0f; // Look slightly down

    vehiclePos = (Vector3){ 490.0f, 50.0f, 490.0f };

    // Initialize Ball data so updateBall doesn't crash on frame 1
    gameBall.velocity = {0,0,0};
    gameBall.radius = 1.0f;

    // Add these inside Game::Game()
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };    // Camera up vector (usually Z or Y)
    camera.fovy = 60.0f;                          // Camera field-of-view aperture
    camera.projection = CAMERA_PERSPECTIVE;      // Camera projection type

    setupResources();
    setupUI();
    
    // Now snap player to ground after map is loaded in setupResources
    playerPos.y = getMapHeightAt(playerPos.x, playerPos.z);
    
    currentState = GameState::Playing;
}

// Helper functions
float Game::getMapHeightAt(float x, float z) {
    Ray ray = { { x, 1000.0f, z }, { 0, -1, 0 } }; 
    
    // We access the first mesh of the model directly
    // This function is guaranteed to exist based on your error log
    RayCollision hit = GetRayCollisionMesh(ray, mapModel.meshes[0], mapModel.transform);
    
    return (hit.hit) ? hit.point.y : 0.0f;
}

Vector3 Game::getMapNormalAt(float x, float z) {
    Ray ray = { { x, 1000.0f, z }, { 0, -1, 0 } };
    
    RayCollision hit = GetRayCollisionMesh(ray, mapModel.meshes[0], mapModel.transform);
    
    return (hit.hit) ? hit.normal : (Vector3){ 0, 1, 0 };
}

void Game::updateBall(float deltaTime) {
    // 1. Apply Gravity
    gameBall.velocity.y -= 15.0f * deltaTime;

    // 2. Air Friction (Damping) - Slows it down over time
    gameBall.velocity = Vector3Scale(gameBall.velocity, 0.995f);

    // 3. Update Position
    gameBall.position = Vector3Add(gameBall.position, Vector3Scale(gameBall.velocity, deltaTime));

    // 4. Ground Collision
    float terrainHeight = getMapHeightAt(gameBall.position.x, gameBall.position.z);
    if (gameBall.position.y - gameBall.radius < terrainHeight) {
        gameBall.position.y = terrainHeight + gameBall.radius;
        
        // Reflect velocity based on ground normal for realistic bounces
        Vector3 normal = getMapNormalAt(gameBall.position.x, gameBall.position.z);
        gameBall.velocity = Vector3Reflect(gameBall.velocity, normal);
        
        // Apply bounciness
        gameBall.velocity = Vector3Scale(gameBall.velocity, gameBall.restitution);
    }

    // 5. Wall Collisions (Boundary 500x500)
    const float limit = 495.0f;
    if (fabs(gameBall.position.x) > limit) {
        gameBall.velocity.x *= -gameBall.restitution;
        gameBall.position.x = (gameBall.position.x > 0) ? limit : -limit;
    }
    if (fabs(gameBall.position.z) > limit) {
        gameBall.velocity.z *= -gameBall.restitution;
        gameBall.position.z = (gameBall.position.z > 0) ? limit : -limit;
    }
}

// Pause Menu setup
void Game::setupUI() {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float centerX = sw * 0.5f;
    float centerY = sh * 0.5f;

    // 1. Pause Menu Box (25% width, 60% height)
    Vector2 menuSize = { sw * 0.25f, sh * 0.6f };
    pauseMenuRect = { centerX - menuSize.x/2, centerY - menuSize.y/2, menuSize.x, menuSize.y };

    // 2. Buttons (15% width, 6% height)
    Vector2 btnSize = { sw * 0.15f, sh * 0.06f };
    resumeBtnRect = { centerX - btnSize.x/2, centerY - (menuSize.y * 0.05f) - btnSize.y/2, btnSize.x, btnSize.y };
    exitBtnRect = { centerX - btnSize.x/2, centerY + (menuSize.y * 0.1f) - btnSize.y/2, btnSize.x, btnSize.y };

    // 3. Slider Track
    Vector2 trackSize = { sw * 0.15f, sh * 0.005f };
    sliderTrackRect = { centerX - trackSize.x/2, centerY + (menuSize.y * 0.3f), trackSize.x, trackSize.y };

    // 4. Slider Handle
    Vector2 handleSize = { sw * 0.01f, sh * 0.03f };
    
    // Use the CENTER of the track Y for the handle Y
    sliderHandleRect = { 
        sliderTrackRect.x + (sliderValue * sliderTrackRect.width) - handleSize.x/2, 
        sliderTrackRect.y + (sliderTrackRect.height / 2.0f) - (handleSize.y / 2.0f), 
        handleSize.x, 
        handleSize.y 
    };
}

// Load in map, models and textures
void Game::setupResources() {
    // 1. Load the Map Model
    mapModel = LoadModel("assets/maps/Towers/Towers.obj");
    grassTexture = LoadTexture("assets/textures/grass.jpg");
    rockTexture = LoadTexture("assets/textures/black-stone.jpg");

    // 2. Load the Shader
    Shader terrainShader = LoadShader("assets/shaders/terrain.vs", "assets/shaders/terrain.fs");

    // Link textures to the shader's sampler2D slots
    int texGrassLoc = GetShaderLocation(terrainShader, "texture0");
    int texRockLoc = GetShaderLocation(terrainShader, "texture1");

    // Assign the shader to the map material
    mapModel.materials[0].shader = terrainShader;
    
    // Slot 0 is always MATERIAL_MAP_DIFFUSE
    mapModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = grassTexture;
    
    // Slot 1 is extra (MATERIAL_MAP_SPECULAR or just custom)
    mapModel.materials[0].maps[MATERIAL_MAP_SPECULAR].texture = rockTexture;
    
    // Tell the shader that sampler2D 'texture1' corresponds to texture slot 1
    int secondSlot = 1;
    SetShaderValue(terrainShader, texRockLoc, &secondSlot, SHADER_UNIFORM_INT);

    // Load player
    playerModel = LoadModel("assets/characters/sci_fi_soldier_light_infantry_low_poly_character.glb");
    // 2. Load the animations from the SAME FBX
    // Load the animations
    playerAnims = LoadModelAnimations("assets/characters/sci_fi_soldier_light_infantry_low_poly_character.glb", &animsCount);

    // Inside setupResources()
    for (int i = 0; i < playerModel.materialCount; i++) {
        // This resets the material to the standard raylib internal shader
        playerModel.materials[i].shader = mapModel.materials[0].shader; 
        // OR simply:
        // playerModel.materials[i].shader = LoadShader(0, 0); 
        
        playerModel.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    }

    // CRITICAL SAFETY CHECK
    if (playerAnims == nullptr) {
        TraceLog(LOG_ERROR, "ANIMATION FILE NOT FOUND! Check your path.");
        animsCount = 0; // Ensure the loop in run() doesn't execute
    }

    vehicleModel = LoadModel("assets/vehicles/spaceship.glb");


    // Load Ball
    gameBall.position = (Vector3){ 0.0f, 300.0f, 0.0f }; // Start in the air
    gameBall.velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    gameBall.radius = 2.0f;
    gameBall.restitution = 0.8f; // Bounces back with 80% energy

    // 2. Load Templates
    Model fenceModel = LoadModel("assets/objects/Farm Buildings - Sept 2018/OBJ/Fence.obj");
    Texture2D woodTex = LoadTexture("assets/textures/wood.png");
    fenceModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = woodTex;

    Model treeModel = LoadModel("assets/objects/Ultimate Nature Pack - Jun 2019/OBJ/CommonTree_5.obj");
    Texture2D leafTex = LoadTexture("assets/textures/leaves.png");
    treeModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = leafTex;

    // 3. FENCE LOOP
    for (int i = 0; i < 4000; i += 6) {
        GameObject f;
        f.model = fenceModel;
        f.scale = { 1.0f, 1.0f, 1.0f };

        // Position Logic
        if (i < 1000) {
            f.position = { 498.0f - (float)i, 0.0f, 498.0f };
            f.rotation = { 0, 0, 0 };
        } else if (i < 2000) {
            f.position = { -498.0f, 0.0f, 498.0f - (float)(i - 1000) };
            f.rotation = { 0, 90.0f, 0 };
        } else if (i < 3000) {
            f.position = { -498.0f + (float)(i - 2000), 0.0f, -498.0f };
            f.rotation = { 0, 0, 0 };
        } else {
            f.position = { 498.0f, 0.0f, -498.0f + (float)(i - 3000) };
            f.rotation = { 0, 90.0f, 0 };
        }

        // 2. Get the ground normal for this spot
        Vector3 normal = getMapNormalAt(f.position.x, f.position.z);
        
        // 3. Store the normal so we can use it in the draw loop
        f.groundNormal = normal;

        // Snap to terrain height
        f.position.y = getMapHeightAt(f.position.x, f.position.z);
        sceneObjects.push_back(f);

        // Every 500 fences, tell the OS we are still working
        if (i % 500 == 0) {
            PollInputEvents(); // Keeps the window responsive during the heavy loop
        }
    }

    // 4. TREE LOOP
    for (int i = 0; i < 50; i++) {
        GameObject t;
        t.model = treeModel;
        t.isTree = true;

        float rx = -100.0f + (float)(-(rand() % 375));
        float rz = 100.0f + (float)(rand() % 375);
        float groundY = getMapHeightAt(rx, rz);
        
        t.position = { rx, groundY, rz };
        
        // Random Scale & Rotation
        float s = 10.0f + (float)(rand() % 201) / 10.0f;
        t.scale = { s, s, s };
        t.rotation = { 0, (float)(rand() % 360), 0 };

        // Slope Alignment Logic (Optional in Raylib - simpler to just set position)
        t.groundNormal = getMapNormalAt(rx, rz);

        sceneObjects.push_back(t);
    }

    // // Windmill
    // GameObject tower;
    // tower.model = LoadModel("assets/objects/Farm Buildings - Sept 2018/OBJ/TowerWindmill.obj");
    // tower.texture = LoadTexture("assets/textures/wood.png");
    // tower.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tower.texture; // Apply texture
    // tower.position = { 400.0f, getMapHeightAt(400.0f, -400.0f), -400.0f };
    // tower.scale = { 15.0f, 15.0f, 15.0f };
    // tower.rotation = { 0, -45.0f, 0 };
    // sceneObjects.push_back(tower);

    // // Barn
    // GameObject barn;
    // barn.model = LoadModel("assets/objects/Farm Buildings - Sept 2018/OBJ/OpenBarn.obj");
    // barn.texture = LoadTexture("assets/textures/wood.png");
    // barn.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = barn.texture; // Apply texture
    // barn.position = { -400.0f, getMapHeightAt(-400.0f, -400.0f), -400.0f };
    // barn.scale = { 15.0f, 15.0f, 15.0f };
    // barn.rotation = { 0, 45.0f, 0 };
    // sceneObjects.push_back(barn);
}

// Process key presses and events
void Game::processEvents(float deltaTime) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (currentState == GameState::Playing) {
            currentState = GameState::Paused;
            EnableCursor();
        } else {
            currentState = GameState::Playing;
            DisableCursor();
        }
    }

    if (currentState == GameState::Playing) {
        // 1. MOUSE LOOK
        Vector2 mouseDelta = GetMouseDelta();
        cameraYaw   -= (mouseDelta.x * sensitivity);
        cameraPitch -= (mouseDelta.y * sensitivity);
        cameraPitch = Clamp(cameraPitch, -89.0f, 89.0f);

        // 2. TOGGLES
        if (IsKeyPressed(KEY_C)) isCrouching = !isCrouching;
        if (IsKeyPressed(KEY_LEFT_SHIFT)) isSprinting = !isSprinting;
        if (IsKeyPressed(KEY_G)) {
            isCreativeMode = !isCreativeMode;
            verticalVelocity = 0.0f;
        }

        // 3. BASE CALCULATIONS
        float floorY = getMapHeightAt(playerPos.x, playerPos.z);
        float baseSpeed = isCreativeMode ? 90.0f : 7.0f;
        float targetMult = (isSprinting) ? 1.7f : (isCrouching ? 0.4f : 1.0f);
        speedMultiplier = Lerp(speedMultiplier, targetMult, 12.0f * deltaTime);
        float currentSpeed = baseSpeed * speedMultiplier;

        // Direction Vectors
        Vector3 forward2D = { sinf(cameraYaw * DEG2RAD), 0, cosf(cameraYaw * DEG2RAD) };
        Vector3 right = { cosf(cameraYaw * DEG2RAD), 0, -sinf(cameraYaw * DEG2RAD) };
        Vector3 forward3D = { 
            sinf(cameraYaw * DEG2RAD) * cosf(cameraPitch * DEG2RAD), 
            sinf(cameraPitch * DEG2RAD), 
            cosf(cameraYaw * DEG2RAD) * cosf(cameraPitch * DEG2RAD) 
        };

        Vector3 moveStep = { 0, 0, 0 };

        // 4. MOVEMENT & PHYSICS BRANCHING
        if (isCreativeMode) {
            // --- CREATIVE FLYING MODE ---
            if (IsKeyDown(KEY_W)) moveStep = Vector3Add(moveStep, forward3D);
            if (IsKeyDown(KEY_S)) moveStep = Vector3Subtract(moveStep, forward3D);
            if (IsKeyDown(KEY_A)) moveStep = Vector3Add(moveStep, right);
            if (IsKeyDown(KEY_D)) moveStep = Vector3Subtract(moveStep, right);

            if (Vector3Length(moveStep) > 0) {
                moveStep = Vector3Normalize(moveStep);
                Vector3 displacement = Vector3Scale(moveStep, currentSpeed * deltaTime);
                
                playerPos.x += displacement.x;
                playerPos.z += displacement.z;

                // Move vertically based on mouse pitch, but block if hitting the floor
                float potentialY = playerPos.y + displacement.y;
                playerPos.y = (potentialY > floorY) ? potentialY : floorY + 0.01f;
            }

            // Vertical Overrides
            if (IsKeyDown(KEY_SPACE)) playerPos.y += currentSpeed * deltaTime;
            if (IsKeyDown(KEY_LEFT_CONTROL)) {
                playerPos.y -= currentSpeed * deltaTime;
                if (playerPos.y < floorY) playerPos.y = floorY + 0.01f;
            }

            isGrounded = true;
            verticalVelocity = 0.0f;
        } 
        else {
            // --- STANDARD WALKING MODE ---
            if (IsKeyDown(KEY_W)) moveStep = Vector3Add(moveStep, forward2D);
            if (IsKeyDown(KEY_S)) moveStep = Vector3Subtract(moveStep, forward2D);
            if (IsKeyDown(KEY_A)) moveStep = Vector3Add(moveStep, right);
            if (IsKeyDown(KEY_D)) moveStep = Vector3Subtract(moveStep, right);

            if (Vector3Length(moveStep) > 0) {
                moveStep = Vector3Normalize(moveStep);
                playerPos = Vector3Add(playerPos, Vector3Scale(moveStep, currentSpeed * deltaTime));
            }

            // Gravity & Jumping
            if (!isGrounded) verticalVelocity -= 18.0f * deltaTime;
            playerPos.y += verticalVelocity * deltaTime;

            if (IsKeyPressed(KEY_SPACE) && isGrounded) {
                verticalVelocity = 8.0f;
                isGrounded = false;
            }

            // Snapping to Slopes
            float snapDistance = 0.5f; 
            if (verticalVelocity <= 0 && (playerPos.y - floorY) < snapDistance) {
                if (getMapNormalAt(playerPos.x, playerPos.z).y >= 0.65f) {
                    playerPos.y = Lerp(playerPos.y, floorY, 7.0f * deltaTime);
                    verticalVelocity = 0.0f;
                    isGrounded = true;
                } else {
                    // Slide down steep slopes
                    Vector3 normal = getMapNormalAt(playerPos.x, playerPos.z);
                    playerPos.x += normal.x * 10.0f * deltaTime;
                    playerPos.z += normal.z * 10.0f * deltaTime;
                    isGrounded = false;
                }
            } else {
                isGrounded = false;
            }
        }

        // --- 5. CAMERA & BOUNDARIES ---
        // Map boundaries
        const float mapLimit = 497.5f; 
        playerPos.x = Clamp(playerPos.x, -mapLimit, mapLimit);
        playerPos.z = Clamp(playerPos.z, -mapLimit, mapLimit);

        // Height Interpolation for Crouching
        float targetEyeHeight = isCrouching ? 0.8f : 1.5f;
        currentEyeHeight = Lerp(currentEyeHeight, targetEyeHeight, 12.0f * deltaTime);

        // --- 6. THIRD PERSON CAMERA POSITIONING ---
        float cameraDistance = 3.0f; 

        // 1. CLAMP PITCH
        cameraPitch = Clamp(cameraPitch, -80.0f, 70.0f); 
        if (cameraPitch > 10.0f) {
            cameraDistance *= 1 - cameraPitch / 70.0f;
        }

        camera.target = (Vector3){ playerPos.x, playerPos.y + currentEyeHeight, playerPos.z };

        // 2. SPHERICAL MATH
        float horizontalDist = cameraDistance * cosf(cameraPitch * DEG2RAD);
        float verticalDist   = cameraDistance * sinf(cameraPitch * DEG2RAD);

        camera.position.x = camera.target.x - sinf(cameraYaw * DEG2RAD) * horizontalDist;
        camera.position.z = camera.target.z - cosf(cameraYaw * DEG2RAD) * horizontalDist;
        camera.position.y = camera.target.y - verticalDist; 

        // 3. FLOOR COLLISION - Only apply if NOT in creative mode
        if (!isCreativeMode) {
            float camFloor = getMapHeightAt(camera.position.x, camera.position.z) + 0.5f;
            if (camera.position.y < camFloor) camera.position.y = camFloor;
            
            // Also ensure player doesn't dive into floor during movement
            if (playerPos.y < floorY) playerPos.y = floorY + 0.01f;
        }

        // --- 7. BALL INTERACTION ---
        float ballDist = Vector3Distance(playerPos, gameBall.position);
        if (ballDist < gameBall.radius + 1.5f) {
            Vector3 pushDir = Vector3Normalize(Vector3Subtract(gameBall.position, playerPos));
            gameBall.velocity = Vector3Add(gameBall.velocity, Vector3Scale(pushDir, 10.0f));
        }

        // --- 8. TREE COLLISION ---
        for (auto& obj : sceneObjects) {
            if (obj.isTree) {
                float dist = Vector2Distance({playerPos.x, playerPos.z}, {obj.position.x, obj.position.z});
                float radius = 2.0f * obj.scale.x / 10.0f;
                if (dist < radius) {
                    Vector2 push = Vector2Scale(Vector2Normalize(Vector2Subtract({playerPos.x, playerPos.z}, {obj.position.x, obj.position.z})), radius - dist);
                    playerPos.x += push.x;
                    playerPos.z += push.y;
                }
            }
        }
    } // Inside the Paused branch of processEvents
    else if (currentState == GameState::Paused) {
        Vector2 mousePos = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(mousePos, resumeBtnRect)) {
                currentState = GameState::Playing;
                DisableCursor();
            }
            if (CheckCollisionPointRec(mousePos, exitBtnRect)) {
                // No easy way to break the loop here, so:
                // Either use a flag or just call exit(0)
                exit(0); 
            }
            if (CheckCollisionPointRec(mousePos, sliderHandleRect)) draggingSlider = true;
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) draggingSlider = false;

        if (draggingSlider) {
            float mouseX = Clamp(GetMousePosition().x, sliderTrackRect.x, sliderTrackRect.x + sliderTrackRect.width);
            
            // 1. Calculate visual 0.0 to 1.0
            sliderValue = (mouseX - sliderTrackRect.x) / sliderTrackRect.width;
            
            // 2. Map that to the math sensitivity (0.01 to 0.2)
            sensitivity = Lerp(0.01f, 0.2f, sliderValue);
        }
    }
}

// Run the game by calling process_events and drawing everything
void Game::run() {
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        
        // Update logic
        processEvents(deltaTime);
        updateBall(deltaTime);

        // // Only update if playerAnims is NOT null and we have at least one animation
        // if (playerAnims != nullptr && animsCount > 0) {
        //     // Safety: Ensure we don't look for an index that doesn't exist
        //     int validAnimIndex = (currentAnimIndex < animsCount) ? currentAnimIndex : 0;
            
        //     ModelAnimation anim = playerAnims[validAnimIndex];
        //     animFrameCounter++;

        //     UpdateModelAnimation(playerModel, anim, animFrameCounter);

        //     if (animFrameCounter >= anim.frameCount) animFrameCounter = 0;
        // }

        BeginDrawing();
            ClearBackground(SKYBLUE);

            BeginMode3D(camera);
                // Draw the Map
                DrawModel(mapModel, {0,0,0}, 1.0f, WHITE);

                float playerRotation = (cameraYaw); 

                // 2. Draw the model
                DrawModelEx(
                    playerModel, 
                    playerPos, 
                    (Vector3){ 0, 1, 0 }, // Rotate around the Y axis
                    playerRotation,       // This is the "steering" angle
                    (Vector3){ 0.01f, 0.01f, 0.01f }, 
                    WHITE
                );

                // DrawModelEx(
                //     vehicleModel, 
                //     playerPos, 
                //     getMapNormalAt(playerPos.x, playerPos.z), // Rotate around the Y axis
                //     playerRotation,       // This is the "steering" angle
                //     (Vector3){ 0.5f, 0.5f, 0.5f }, 
                //     WHITE
                // );

                // 1. The Core (Brightest part)
                DrawSphere(gameBall.position, gameBall.radius, ORANGE);
                DrawSphereWires(gameBall.position, gameBall.radius + 0.1, 10, 10, BLACK);

                // Draw all objects with their specific rotation and scale
                for (auto& obj : sceneObjects) {
                    if (obj.isTree) {
                        // Trees usually grow straight up regardless of slope
                        DrawModelEx(obj.model, obj.position, {0, 1, 0}, obj.rotation.y, obj.scale, WHITE);
                    } else {
                        // Fences should align to the ground normal
                        // Rotate {0,1,0} (default up) to match groundNormal
                        Quaternion q = QuaternionFromVector3ToVector3({0, 1, 0}, obj.groundNormal);
                        
                        // Combine with the fence's path rotation (around the new normal)
                        Quaternion pathRot = QuaternionFromAxisAngle(obj.groundNormal, obj.rotation.y * DEG2RAD);
                        Quaternion finalRot = QuaternionMultiply(pathRot, q);
                        
                        // Convert back to Axis-Angle for DrawModelEx
                        Vector3 axis;
                        float angle;
                        QuaternionToAxisAngle(finalRot, &axis, &angle);
                        
                        DrawModelEx(obj.model, obj.position, axis, angle * RAD2DEG, obj.scale, WHITE);
                    }
                }
            EndMode3D();

            // --- 2D UI LAYER ---
            if (currentState == GameState::Playing) {
                if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                    int centerX = GetScreenWidth() / 2;
                    int centerY = GetScreenHeight() / 2;
                    DrawCircle(centerX, centerY, 4, WHITE); // Clean dot crosshair
                    DrawCircleLines(centerX, centerY, 10, Fade(WHITE, 0.5f)); // Subtle ring
                }
            }

            if (currentState == GameState::Paused) {
                float sw = (float)GetScreenWidth();
                float sh = (float)GetScreenHeight();

                // 1. Dark Overlay
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));

                // 2. Menu Background
                DrawRectangleRec(pauseMenuRect, Color{ 40, 40, 40, 220 });

                // 3. Title Text
                int fontSize = (int)(sh * 0.05f);
                int textWidth = MeasureText("PAUSED", fontSize);
                DrawText("PAUSED", sw/2 - textWidth/2, pauseMenuRect.y + (pauseMenuRect.height * 0.05f), fontSize, WHITE);

                // 4. Buttons
                DrawRectangleRec(resumeBtnRect, Color{ 0, 0, 0, 180 });
                DrawRectangleRec(exitBtnRect, Color{ 0, 0, 0, 180 });

                // 5. Button Labels (Using 60% of button height)
                int labelSize = (int)(resumeBtnRect.height * 0.6f);
                DrawText("RESUME", resumeBtnRect.x + (resumeBtnRect.width/2 - MeasureText("RESUME", labelSize)/2), 
                        resumeBtnRect.y + (resumeBtnRect.height/2 - labelSize/2), labelSize, WHITE);
                
                DrawText("EXIT", exitBtnRect.x + (exitBtnRect.width/2 - MeasureText("EXIT", labelSize)/2), 
                        exitBtnRect.y + (exitBtnRect.height/2 - labelSize/2), labelSize, WHITE);


                // 1. Update handle position based on sliderValue
                sliderHandleRect.x = sliderTrackRect.x + (sliderValue * sliderTrackRect.width) - (sliderHandleRect.width / 2.0f);
                sliderHandleRect.y = sliderTrackRect.y + (sliderTrackRect.height / 2.0f) - (sliderHandleRect.height / 2.0f);

                // 2. Draw Track and Handle
                DrawRectangleRec(sliderTrackRect, GRAY); 
                DrawRectangleRec(sliderHandleRect, WHITE); 

                // 3. Draw "MOUSE SENSITIVITY" Header
                DrawText("MOUSE SENSITIVITY", sw/2 - MeasureText("MOUSE SENSITIVITY", 20)/2, 
                        sliderTrackRect.y - pauseMenuRect.height * 0.05, 20, WHITE);

                // 4. Draw the Value BELOW the slider
                // We show the sliderValue (0.00 to 1.00) here
                const char* sensText = TextFormat("Value: %.2f", sliderValue);
                int sensTextWidth = MeasureText(sensText, 20);
                DrawText(sensText, sw/2 - sensTextWidth/2, sliderTrackRect.y + pauseMenuRect.height * 0.04, 20, WHITE);
            }

        EndDrawing();
    }
}

Game::~Game() {
    // Use 'int' here to match the new type of animsCount
    for (int i = 0; i < animsCount; i++) {
        UnloadModelAnimation(playerAnims[i]);
    }
    RL_FREE(playerAnims); 
    UnloadModel(playerModel);

    UnloadModel(mapModel);
    UnloadTexture(grassTexture);
    UnloadTexture(rockTexture);
    
    // Unload everything in your sceneObjects list if they aren't using the templates
    // But since they use shared models, just unload the main templates you loaded
    CloseWindow();
}