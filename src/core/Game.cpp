#include "Game.hpp"
#include "Physics.hpp"
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

    // 1. LOAD MODELS FIRST
    // Everything else depends on mapModel being valid
    setupResources(); 
    setupUI();

    // 2. NOW initialize the player (after window and map are ready)
    player.Init(); 

    cameraYaw = 225.0f;
    cameraPitch = -20.0f;

    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // 3. NOW calculate ball spawn (mapModel is now loaded!)
    float startX = 480.0f;
    float startZ = 480.0f;
    
    // Safety check: if GetHeight crashes here, mapModel loading failed
    float groundAtSpawn = Physics::GetHeight({startX, 300.0f, startZ}, mapModel);

    gameBall.position = (Vector3){ startX, groundAtSpawn + 50.0f, startZ };
    gameBall.velocity = (Vector3){ 0, 0, 0 };
    gameBall.radius = 2.0f;
    gameBall.restitution = 0.5f;

    currentState = GameState::Playing;
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
    terrainShader = LoadShader("assets/shaders/terrain.vs", "assets/shaders/terrain.fs");

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
        f.groundNormal = Physics::GetNormal(f.position, mapModel);

        // Snap to terrain height
        f.position.y = Physics::GetHeight(f.position, mapModel);
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
        float groundY = Physics::GetHeight({rx, 0, rz}, mapModel);
        
        t.position = { rx, groundY, rz };
        
        // Random Scale & Rotation
        float s = 10.0f + (float)(rand() % 201) / 10.0f;
        t.scale = { s, s, s };
        t.rotation = { 0, (float)(rand() % 360), 0 };

        // Slope Alignment Logic (Optional in Raylib - simpler to just set position)
        t.groundNormal = Physics::GetNormal({rx, 0, rz}, mapModel);

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

void Game::processEvents(float deltaTime) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (currentState == GameState::Playing) { currentState = GameState::Paused; EnableCursor(); }
        else { currentState = GameState::Playing; DisableCursor(); }
    }

    if (currentState == GameState::Playing) {
        // 1. MOUSE LOOK (Game handles the "view", Player handles the "body")
        Vector2 mouseDelta = GetMouseDelta();
        cameraYaw   -= (mouseDelta.x * sensitivity);
        cameraPitch -= (mouseDelta.y * sensitivity);
        cameraPitch = Clamp(cameraPitch, -80.0f, 70.0f);

        // 2. UPDATE PLAYER
        // We pass the view angles so the player knows which way to walk
        player.Update(deltaTime, mapModel, cameraYaw, cameraPitch);

        // 3. CAMERA POSITIONING (The "Follow" logic)
        float cameraDistance = 3.0f;
        camera.target = player.GetCameraTarget(); // Use the helper from Player class

        // Spherical Math for 3rd Person Camera
        float horizontalDist = cameraDistance * cosf(cameraPitch * DEG2RAD);
        float verticalDist   = cameraDistance * sinf(cameraPitch * DEG2RAD);

        camera.position.x = camera.target.x - sinf(cameraYaw * DEG2RAD) * horizontalDist;
        camera.position.z = camera.target.z - cosf(cameraYaw * DEG2RAD) * horizontalDist;
        camera.position.y = camera.target.y - verticalDist; 

        // Camera Floor Collision
        float camFloor = Physics::GetHeight(camera.position, mapModel) + 0.5f;
        if (camera.position.y < camFloor) camera.position.y = camFloor;
        
        // 4. BALL INTERACTION
        float dist = Vector3Distance(player.position, gameBall.position);
        if (dist < gameBall.radius + 1.2f) { 
            Vector3 pushDir = Vector3Normalize(Vector3Subtract(gameBall.position, player.position));
            
            // INCREASE THIS: 5.0f is a nudge, 25.0f is a kick
            float kickPower = 25.0f; 
            
            // Apply the horizontal push
            gameBall.velocity.x += pushDir.x * kickPower;
            gameBall.velocity.z += pushDir.z * kickPower;
            
            // Give it a tiny bit of "up" so it doesn't immediately grind into the floor friction
            gameBall.velocity.y = 2.0f; 
        }
    }

    if (currentState == GameState::Paused) {
        Vector2 mousePos = GetMousePosition();

        // Resume Button Logic
        if (CheckCollisionPointRec(mousePos, resumeBtnRect)) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                currentState = GameState::Playing;
                DisableCursor();
            }
        }

        // Exit Button Logic
        if (CheckCollisionPointRec(mousePos, exitBtnRect)) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                // Unload and Close
                exit(0); 
            }
        }
    }
}

// Run the game by calling process_events and drawing everything
void Game::run() {
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        
        // Update logic
        processEvents(deltaTime);
        Physics::UpdateBall(gameBall, mapModel, deltaTime);

        BeginDrawing();
            ClearBackground(SKYBLUE);

            BeginMode3D(camera);
                // Draw the Map
                DrawModel(mapModel, {0,0,0}, 1.0f, WHITE);

               // DRAW PLAYER
                player.Draw(cameraYaw); 

                // DRAW BALL
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

    UnloadModel(mapModel);
    UnloadTexture(grassTexture);
    UnloadTexture(rockTexture);
    
    // Unload everything in your sceneObjects list if they aren't using the templates
    // But since they use shared models, just unload the main templates you loaded
    CloseWindow();
}