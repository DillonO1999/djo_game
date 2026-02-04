#include "Game.hpp"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define TINYOBJLOADER_IMPLEMENTATION // Define this in only *one* .cc file
#include "tiny_obj_loader.h"

// --- SHADER SOURCE CODE (GLSL) ---
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;
    layout (location = 2) in vec3 aNormal; // New

    out vec2 TexCoord;
    out float slope; // Pass steepness to fragment shader

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        TexCoord = aTexCoord;
        // Normal.y tells us how "upward" the surface faces (1.0 = flat, 0.0 = vertical)
        slope = aNormal.y; 
    }
)glsl";

const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor; // <--- ADD THIS LINE
    
    in vec2 TexCoord;
    in float slope;

    uniform sampler2D grassTexture;
    uniform sampler2D rockTexture;

    void main() {
        vec4 grass = texture(grassTexture, TexCoord * 100.0);
        vec4 rock = texture(rockTexture, TexCoord * 100.0);
        float blend = smoothstep(0.6, 0.8, slope);
        FragColor = mix(rock, grass, blend);
    }
)glsl";

const char* objectFragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;
    uniform sampler2D objectTexture;

    void main() {
        vec4 texColor = texture(objectTexture, TexCoord);
        if(texColor.a < 0.1) discard; // Support for transparent fences/leaves
        FragColor = texColor;
    }
)glsl";

Game::Game() : pauseText(font), resumeText(font), exitText(font), sensitivityText(font) {
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 3;
    settings.minorVersion = 3;
    settings.attributeFlags = sf::ContextSettings::Default;

    // SFML 3 uses sf::State::Windowed instead of sf::Style::Default
    window.create(sf::VideoMode(sf::VideoMode::getDesktopMode().size), "Real 3D - First Triangle", sf::State::Windowed, settings);

    if (!gladLoadGLLoader((GLADloadproc)sf::Context::getFunction)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
    }

    camera.position = glm::vec3(490.0f, 40.0f, 490.0f); // 2 meters high, slightly back from center

    setupResources();

    windowCenter = sf::Vector2i(window.getSize().x / 2, window.getSize().y / 2);
    window.setMouseCursorVisible(false); // Hide the cursor
    sf::Mouse::setPosition(windowCenter, window); // Reset to center

    if (!font.openFromFile("assets/fonts/BBH_Bogle/BBHBogle-Regular.ttf")) {
        std::cerr << "Error: Could not load font file!" << std::endl;
        return;
    }

    setupUI();
    window.resetGLStates(); // <-- ADD THIS
    currentState = GameState::Playing; // Start the game in playing mode
}

float Game::getMapHeightAt(float x, float z) {
    float highestY = -100.0f; // Default "void" height
    bool foundFloor = false;

    // Loop through triangles (3 vertices at a time, 5 floats per vertex)
    for (size_t i = 0; i < mapVertices.size(); i += 24) {
        glm::vec3 v0(mapVertices[i],     mapVertices[i+1], mapVertices[i+2]);
        glm::vec3 v1(mapVertices[i+8],   mapVertices[i+9], mapVertices[i+10]);
        glm::vec3 v2(mapVertices[i+16],  mapVertices[i+17], mapVertices[i+18]);
        
        // 1. Barycentric Coordinate check: Is (x, z) inside this triangle?
        float det = (v1.z - v2.z) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.z - v2.z);
        float l1 = ((v1.z - v2.z) * (x - v2.x) + (v2.x - v1.x) * (z - v2.z)) / det;
        float l2 = ((v2.z - v0.z) * (x - v2.x) + (v0.x - v2.x) * (z - v2.z)) / det;
        float l3 = 1.0f - l1 - l2;

        if (l1 >= 0 && l2 >= 0 && l3 >= 0) {
            // 2. If inside, calculate the Y height at this specific point
            float h = l1 * v0.y + l2 * v1.y + l3 * v2.y;
            highestY = std::max(highestY, h);
            foundFloor = true;
        }
    }

    return foundFloor ? highestY : 0.0f; 
}

void Game::loadMap(const std::string& path) {

    if (mapVAO != 0) glDeleteVertexArrays(1, &mapVAO);
    if (mapVBO != 0) glDeleteBuffers(1, &mapVBO);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        std::cerr << "OBJ Loader Error: " << warn << err << std::endl;
        return;
    }

    mapVertices.clear();

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            // 1. Positions
            float vx = attrib.vertices[3 * index.vertex_index + 0];
            float vy = attrib.vertices[3 * index.vertex_index + 1];
            float vz = attrib.vertices[3 * index.vertex_index + 2];
            mapVertices.push_back(vx);
            mapVertices.push_back(vy);
            mapVertices.push_back(vz);

            // 2. Textures (UVs) - Calculate tx and ty here!
            float tx = 0.0f, ty = 0.0f;
            if (index.texcoord_index >= 0) {
                tx = attrib.texcoords[2 * index.texcoord_index + 0];
                ty = attrib.texcoords[2 * index.texcoord_index + 1];
            }
            mapVertices.push_back(tx);
            mapVertices.push_back(ty);

            // 3. Normals
            if (index.normal_index >= 0) {
                mapVertices.push_back(attrib.normals[3 * index.normal_index + 0]);
                mapVertices.push_back(attrib.normals[3 * index.normal_index + 1]);
                mapVertices.push_back(attrib.normals[3 * index.normal_index + 2]);
            } else {
                mapVertices.push_back(0.0f); mapVertices.push_back(1.0f); mapVertices.push_back(0.0f);
            }
        }
    }

    // --- OpenGL Setup ---
    glGenVertexArrays(1, &mapVAO);
    glGenBuffers(1, &mapVBO);

    glBindVertexArray(mapVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mapVBO);
    glBufferData(GL_ARRAY_BUFFER, mapVertices.size() * sizeof(float), mapVertices.data(), GL_STATIC_DRAW);

    // Position (Location 0): 3 floats, starts at 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture (Location 1): 2 floats, starts after 3 Pos floats
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Normals (Location 2): 3 floats, starts after 3 Pos + 2 UV floats
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Unbind to prevent accidental overrides
    glBindVertexArray(0); 
}

void Game::setupResources() {
    // Compile shaders
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int ofs = compileShader(GL_FRAGMENT_SHADER, objectFragmentShaderSource);

    // --- TERRAIN SHADER ---
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    
    // Set terrain texture units (Do this once!)
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "grassTexture"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "rockTexture"), 1);

    // --- OBJECT SHADER ---
    objectShaderProgram = glCreateProgram();
    glAttachShader(objectShaderProgram, vs); // Reuse the same vertex logic
    glAttachShader(objectShaderProgram, ofs);
    glLinkProgram(objectShaderProgram);

    // Set object texture unit (Do this once!)
    glUseProgram(objectShaderProgram);
    glUniform1i(glGetUniformLocation(objectShaderProgram, "objectTexture"), 0);

    // Cleanup
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteShader(ofs);

    loadMap("assets/maps/Towers/Towers.obj");

    // Load Grass Texture
    glGenTextures(1, &grassTextureID);
    glBindTexture(GL_TEXTURE_2D, grassTextureID);
    // ... Set parameters (Wrap/Filter) ...
    sf::Image grassImg;
    if (grassImg.loadFromFile("assets/textures/grass.jpg")) { // Make sure this path exists!
        grassImg.flipVertically();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, grassImg.getSize().x, grassImg.getSize().y, 0, GL_RGBA, GL_UNSIGNED_BYTE, grassImg.getPixelsPtr());
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    // Load Rock Texture
    glGenTextures(1, &rockTextureID);
    glBindTexture(GL_TEXTURE_2D, rockTextureID);
    // ... Set parameters (Wrap/Filter) ...
    sf::Image rockImg;
    if (rockImg.loadFromFile("assets/textures/black-stone.jpg")) { // Make sure this path exists!
        rockImg.flipVertically();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rockImg.getSize().x, rockImg.getSize().y, 0, GL_RGBA, GL_UNSIGNED_BYTE, rockImg.getPixelsPtr());
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    // 3. LOAD TEMPLATES ONCE (The "Heavy" part)
    GameObject fenceTemplate;
    loadObject(fenceTemplate, "assets/objects/Farm Buildings - Sept 2018/OBJ/Fence.obj", "assets/textures/wood.png");

    GameObject treeTemplate;
    loadObject(treeTemplate, "assets/objects/Ultimate Nature Pack - Jun 2019/OBJ/CommonTree_5.obj", "assets/textures/leaves.png");

    // 4. THE FENCE LOOP (The "Fast" part)
    for (int i = 0; i < 4000; i += 6) {
        GameObject f = fenceTemplate; // Copying the VAO/VBO IDs, not reloading files

        // Use corrected C++ logic: (i >= min && i < max)
        if (i >= 0 && i < 1000) {
            f.position = glm::vec3(498.0f - static_cast<float>(i), 0.0f, 498.0f);
            f.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        } else if (i >= 1000 && i < 2000) {
            f.position = glm::vec3(-498.0f, 0.0f, 498.0f - static_cast<float>(i - 1000));
            f.rotation = glm::vec3(0.0f, 90.0f, 0.0f); // Rotate Y to turn the corner
        } else if (i >= 2000 && i < 3000) {
            f.position = glm::vec3(-498.0f + static_cast<float>(i - 2000), 0.0f, -498.0f);
            f.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        } else if (i >= 3000 && i < 4000) {
            f.position = glm::vec3(498.0f, 0.0f, -498.0f + static_cast<float>(i - 3000));
            f.rotation = glm::vec3(0.0f, 90.0f, 0.0f);
        }

        // 2. FETCH the data from the map (Height and Normal)
        f.position.y = getMapHeightAt(f.position.x, f.position.z);
        glm::vec3 groundNormal = getMapNormalAt(f.position.x, f.position.z); // Create it here!
        f.groundNormal = groundNormal; // Store it for later

        // 3. DO the math using that groundNormal
        glm::vec3 worldUp(0, 1, 0);
        float dot = glm::clamp(glm::dot(worldUp, groundNormal), -1.0f, 1.0f);
        float angle = acos(dot);
        glm::vec3 axis = glm::cross(worldUp, groundNormal);

        glm::mat4 slopeRotation = glm::mat4(1.0f);
        if (glm::length(axis) > 0.001f) {
            slopeRotation = glm::rotate(glm::mat4(1.0f), angle, glm::normalize(axis));
        }

        // 4. BUILD the final matrix
        f.modelMatrix = glm::translate(glm::mat4(1.0f), f.position) * slopeRotation * glm::rotate(glm::mat4(1.0f), glm::radians(f.rotation.y), worldUp) * glm::scale(glm::mat4(1.0f), f.scale);

        // 5. FINALLY push to the list
        sceneObjects.push_back(f);
    }

    // 5. THE TREE LOOP
    for (int i = 0; i < 50; i++) {
        GameObject t = treeTemplate;
        float rx = -100.0f + static_cast<float>(-(rand() % 375));
        float rz = 100.0f + static_cast<float>(rand() % 375);

        // 1. Get Height and Normal
        float groundY = getMapHeightAt(rx, rz);
        glm::vec3 groundNormal = getMapNormalAt(rx, rz);
        
        t.position = glm::vec3(rx, groundY, rz);
        
        // 2. Random Scale & Rotation
        float s = 10.0f + static_cast<float>(rand() % 201) / 10.0f;
        t.scale = glm::vec3(s, s, s);
        float randomYaw = static_cast<float>(rand() % 360);

        // 3. Calculate Slope Matrix (Just like the fences!)
        glm::vec3 worldUp(0, 1, 0);
        float dot = glm::clamp(glm::dot(worldUp, groundNormal), -1.0f, 1.0f);
        float angle = acos(dot);
        glm::vec3 axis = glm::cross(worldUp, groundNormal);

        glm::mat4 slopeRotation = glm::mat4(1.0f);
        if (glm::length(axis) > 0.001f) {
            slopeRotation = glm::rotate(glm::mat4(1.0f), angle, glm::normalize(axis));
        }

        // 4. Final Matrix Construction
        t.modelMatrix = glm::translate(glm::mat4(1.0f), t.position) * slopeRotation * glm::rotate(glm::mat4(1.0f), glm::radians(randomYaw), worldUp) * glm::scale(glm::mat4(1.0f), t.scale);
        t.isTree = true; 
        
        sceneObjects.push_back(t);
    }

    GameObject TowerWindmill;
    loadObject(TowerWindmill, "assets/objects/Farm Buildings - Sept 2018/OBJ/TowerWindmill.obj", "assets/textures/wood.png");
    TowerWindmill.position = glm::vec3(400.0f, getMapHeightAt(400.0f, -400.0f), -400.0f);
    TowerWindmill.scale = glm::vec3(15.0f,15.0f,15.0f);
    TowerWindmill.rotation = glm::vec3(0.0f, -45.0f, 0.0f);
    sceneObjects.push_back(TowerWindmill);

    GameObject Barn;
    loadObject(Barn, "assets/objects/Farm Buildings - Sept 2018/OBJ/OpenBarn.obj", "assets/textures/wood.png");
    Barn.position = glm::vec3(-400.0f, getMapHeightAt(-400.0f, -400.0f), -400.0f);
    Barn.scale = glm::vec3(15.0f,15.0f,15.0f);
    Barn.rotation = glm::vec3(0.0f, 45.0f, 0.0f);
    sceneObjects.push_back(Barn);
}

unsigned int Game::compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    // Check for compilation errors
    int success;
    char infoLog[512];
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(id, 512, NULL, infoLog);
        std::cerr << "ERROR: Shader Compilation Failed (" 
                  << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") 
                  << ")\n" << infoLog << std::endl;
    }
    return id;
}

void Game::run() {
    sf::Clock clock;

    glDisable(GL_CULL_FACE);
    
    // Enable Depth Testing so objects behind others are hidden correctly
    glEnable(GL_DEPTH_TEST); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
        processEvents(deltaTime);

        // 1. Clear the screen AND the depth buffer
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glEnable(GL_DEPTH_TEST); // Ensure this is on
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

        // 2. IMPORTANT: Reset Depth Mask before drawing 3D
        glDepthMask(GL_TRUE); 
        glDepthFunc(GL_LESS);

        glUseProgram(shaderProgram);

        // Re-enable these!
        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        unsigned int viewLoc  = glGetUniformLocation(shaderProgram, "view");
        unsigned int projLoc  = glGetUniformLocation(shaderProgram, "projection");

        glm::mat4 view = camera.getViewMatrix();
        float aspect = window.getSize().x / (float)window.getSize().y;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 1.0f, 10000.0f);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(mapVAO);
        glm::mat4 model = glm::mat4(1.0f); 
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTextureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "grassTexture"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, rockTextureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "rockTexture"), 1);

        glDrawArrays(GL_TRIANGLES, 0, mapVertices.size() / 8); // Divide by 8 now!

        // 2. Draw all other objects
        for (auto& obj : sceneObjects) {
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(obj.getModelMatrix()));
            glBindVertexArray(obj.VAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, obj.textureID);
            
            // Note: We use a different uniform for object texture since map uses two textures
            // You might want a simpler "StaticObjectShader" for single-texture objects
            glDrawArrays(GL_TRIANGLES, 0, obj.vertexCount);
        }

        window.resetGLStates();

        if (currentState == GameState::Paused) {
            // 1. Tell the GPU to stop using your 3D shader
            glUseProgram(0); 
            glBindVertexArray(0);

            // 2. Save 3D state and reset for SFML
            window.pushGLStates(); 

            // 3. Clear depth and reset view for 2D
            glClear(GL_DEPTH_BUFFER_BIT);
            window.setView(window.getDefaultView());

            // Draw boxes first
            window.draw(pauseOverlay);
            window.draw(pauseMenu);
            window.draw(resumeBtn);
            window.draw(exitBtn);

            // Draw text on top of boxes
            window.draw(pauseText);
            window.draw(resumeText);
            window.draw(exitText);

            // Draw slider last
            window.draw(sensitivityText);
            window.draw(sensitivityTrack);
            window.draw(sensitivityHandle);

            // 5. Restore 3D settings
            window.popGLStates();
        }
        // 4. Show the result
        window.display();
    }
}

void Game::processEvents(float deltaTime) {
    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) window.close();
        
        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            // Global Toggles
            // Handle Pause Toggle
            if (keyPressed->code == sf::Keyboard::Key::Escape) {
                if (currentState == GameState::Playing) {
                    currentState = GameState::Paused;
                    window.setMouseCursorVisible(true);
                } else {
                    currentState = GameState::Playing;
                    window.setMouseCursorVisible(false);
                }
            }

            // --- CROUCH TOGGLE ---
            if (keyPressed->code == sf::Keyboard::Key::C && currentState == GameState::Playing) {
                camera.isCrouching = !camera.isCrouching; // Flip the state
            }

            // --- SPRINT TOGGLE ---
            if (keyPressed->code == sf::Keyboard::Key::LShift && currentState == GameState::Playing) {
                camera.isSprinting = !camera.isSprinting; // Flip the state
            }
            
            // TODO: make this a button in pause menu
            if (keyPressed->code == sf::Keyboard::Key::G && currentState == GameState::Playing) {
                isCreativeMode = !isCreativeMode;
                // Reset velocity when entering/exiting to prevent "sliding"
                camera.verticalVelocity = 0.0f; 
            }
        }
    }

    
    if (currentState == GameState::Playing) {
        // --- 1. SET DYNAMIC SPEED ---
        float baseSpeed = isCreativeMode ? 90.0f : 7.0f;
        float targetMult = 1.0f;
        if (camera.isSprinting) targetMult = 1.7f;
        if (camera.isCrouching) targetMult = 0.4f;

        camera.speedMultiplier = glm::mix(camera.speedMultiplier, targetMult, 3.0f * deltaTime);
        camera.speed = baseSpeed * camera.speedMultiplier;

        // 2. JUMP INPUT (Move this UP!)
        if (!isCreativeMode && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) && camera.isGrounded) {
            camera.verticalVelocity = 7.0f;
            camera.isGrounded = false;
        }

        // --- 2. PRE-MOVEMENT CHECK ---
        glm::vec3 oldPos = camera.position;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) camera.processKeyboard('W', deltaTime, isCreativeMode);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) camera.processKeyboard('S', deltaTime, isCreativeMode);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) camera.processKeyboard('A', deltaTime, isCreativeMode);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) camera.processKeyboard('D', deltaTime, isCreativeMode);

        if (!isCreativeMode) {
            glm::vec3 nextNormal = getMapNormalAt(camera.position.x, camera.position.z);
            const float slopeLimit = 0.6f;
            if (nextNormal.y < slopeLimit && camera.isGrounded) {
                camera.position.x = oldPos.x;
                camera.position.z = oldPos.z;
            }
        }

        // 4. PHYSICS PREP
        float terrainHeight = getMapHeightAt(camera.position.x, camera.position.z);
        glm::vec3 groundNormal = getMapNormalAt(camera.position.x, camera.position.z);
        
        // SMOOTH CROUCH FIX: Interpolate the OFFSET, not the total Y
        float targetEyeHeight = camera.isCrouching ? 0.8f : 1.5f;
        camera.currentEyeHeight = glm::mix(camera.currentEyeHeight, targetEyeHeight, 8.0f * deltaTime);
        
        float floorY = terrainHeight + camera.currentEyeHeight;

        // 5. VERTICAL PHYSICS (Apply gravity BEFORE checking collision)
        if (isCreativeMode) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)) camera.position.y += camera.speed * deltaTime;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl)) camera.position.y -= camera.speed * deltaTime;
            if (camera.position.y < floorY) camera.position.y = floorY;
            camera.verticalVelocity = 0.0f;
        } else {
            // Apply Gravity first
            if (!camera.isGrounded) {
                camera.verticalVelocity -= 15.0f * deltaTime;
            }
            camera.position.y += camera.verticalVelocity * deltaTime;

            // --- 6. GROUND COLLISION & ILLEGAL SLOPE CHECK ---
            const float slopeLimit = 0.6f;
            float snapDistance = 0.5f;

            // Check if we are hitting the floor
            bool hittingFloor = (camera.position.y <= floorY);
            // Check if we are close enough to snap (downhill running)
            bool shouldSnap = (camera.isGrounded && camera.position.y <= floorY + snapDistance && camera.verticalVelocity <= 0.1f);

            if (hittingFloor || shouldSnap) {
                // We are touching the terrain... but is it a walkable slope?
                if (groundNormal.y >= slopeLimit) {
                    // VALID GROUND: Snap to floor and stop falling
                    camera.position.y = floorY;
                    camera.verticalVelocity = 0.0f;
                    camera.isGrounded = true;
                } 
                else {
                    // ILLEGAL SLOPE: Force sliding state
                    camera.isGrounded = false; // This is the key! Never stay grounded here.
                    
                    // Calculate downhill direction
                    glm::vec3 worldDown(0.0f, -1.0f, 0.0f);
                    glm::vec3 slideDir = worldDown - (groundNormal * glm::dot(worldDown, groundNormal));
                    
                    if (glm::length(slideDir) > 0.001f) {
                        slideDir = glm::normalize(slideDir);
                        float slideSpeed = 15.0f; // Fast enough to overpower walking
                        camera.position.x += slideDir.x * slideSpeed * deltaTime;
                        camera.position.z += slideDir.z * slideSpeed * deltaTime;
                    }

                    // IMPORTANT: If we hit a steep slope, we stay slightly above it
                    // so that the gravity code below continues to pull us down.
                    if (camera.position.y < floorY + 0.05f) {
                        camera.position.y = floorY + 0.05f;
                    }
                }
            } 
            else {
                // TRULY IN THE AIR
                camera.isGrounded = false;
            }
        }

        // Mouse Look - CALCULATE EVERY FRAME
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        if (mousePos != windowCenter) { // Only process if the mouse actually moved
            float xoffset = (float)(mousePos.x - windowCenter.x) * sensitivity;
            float yoffset = (float)(windowCenter.y - mousePos.y) * sensitivity; 

            camera.processMouse(xoffset, yoffset);
            sf::Mouse::setPosition(windowCenter, window); // Reset to center
        }

        // --- PERIMETER COLLISION ---
        const float boundary = 497.0f; // Slightly inside the 498 fence line

        // Check X boundary
        if (camera.position.x > boundary)  camera.position.x = boundary;
        if (camera.position.x < -boundary) camera.position.x = -boundary;

        // Check Z boundary
        if (camera.position.z > boundary)  camera.position.z = boundary;
        if (camera.position.z < -boundary) camera.position.z = -boundary;

        // After movement keys, check against every tree
        for (auto& obj : sceneObjects) {
            // We only care about objects that should have collision (like trees)
            // You can add a 'bool hasCollision' to your GameObject struct
            if (obj.isTree) { 
                float dx = camera.position.x - obj.position.x;
                float dz = camera.position.z - obj.position.z;
                float distanceSquared = dx*dx + dz*dz;
                float radius = 2.0f * obj.scale[0] / 10.0f; // The thickness of the tree trunk

                if (distanceSquared < radius * radius) {
                    // Collision detected! Push the player back
                    float distance = sqrt(distanceSquared);
                    float overlap = radius - distance;
                    
                    // Move camera away from tree center
                    camera.position.x += (dx / distance) * overlap;
                    camera.position.z += (dz / distance) * overlap;
                }
            }
        }
    }

    if (currentState == GameState::Paused) {
        // 1. Get the pixel position first
        sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
        
        // 2. Convert it to world/UI coordinates
        sf::Vector2f mouseCoords = window.mapPixelToCoords(pixelPos);

        // Now use mouseCoords for everything else
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            if (resumeBtn.getGlobalBounds().contains(mouseCoords)) {
                currentState = GameState::Playing;
                window.setMouseCursorVisible(false);
                sf::Mouse::setPosition(windowCenter, window);
            }
            
            if (exitBtn.getGlobalBounds().contains(mouseCoords)) {
                window.close();
            }
            
            if (sensitivityHandle.getGlobalBounds().contains(mouseCoords)) {
                draggingSlider = true;
            }
        }

        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            // If clicking the handle, start dragging
            if (sensitivityHandle.getGlobalBounds().contains(mouseCoords)) {
                draggingSlider = true;
            }

            if (draggingSlider) {
               float trackWidth = sensitivityTrack.getSize().x;
    
                // Because the track origin is centered, the left edge is:
                // Position.x MINUS half the width
                float minX = sensitivityTrack.getPosition().x - (trackWidth * 0.5f);
                float maxX = minX + trackWidth;

                // Clamp the mouse X position between the true edges
                float newX = std::clamp(mouseCoords.x, minX, maxX);

                sensitivityHandle.setPosition({newX, sensitivityHandle.getPosition().y});

                // Calculate the 0.0 to 1.0 ratio (t) correctly
                float t = (newX - minX) / trackWidth;

                // Apply to sensitivity range
                float minSens = 0.01f;
                float maxSens = 0.2f;
                sensitivity = minSens + t * (maxSens - minSens);
            }
        } else {
            draggingSlider = false;
        }
    }
}

glm::vec3 Game::getMapNormalAt(float x, float z) {
    // 8 floats per vertex: Pos(3), Tex(2), Norm(3)
    // 3 vertices per triangle = 24 floats
    for (size_t i = 0; i < mapVertices.size(); i += 24) {
        glm::vec3 v0(mapVertices[i],     mapVertices[i+1], mapVertices[i+2]);
        glm::vec3 v1(mapVertices[i+8],   mapVertices[i+9], mapVertices[i+10]);
        glm::vec3 v2(mapVertices[i+16],  mapVertices[i+17], mapVertices[i+18]);

        float det = (v1.z - v2.z) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.z - v2.z);
        float l1 = ((v1.z - v2.z) * (x - v2.x) + (v2.x - v1.x) * (z - v2.z)) / det;
        float l2 = ((v2.z - v0.z) * (x - v2.x) + (v0.x - v2.x) * (z - v2.z)) / det;
        float l3 = 1.0f - l1 - l2;

        if (l1 >= 0 && l2 >= 0 && l3 >= 0) {
            // Normals start at index + 5
            glm::vec3 n0(mapVertices[i+5],  mapVertices[i+6],  mapVertices[i+7]);
            glm::vec3 n1(mapVertices[i+13], mapVertices[i+14], mapVertices[i+15]);
            glm::vec3 n2(mapVertices[i+21], mapVertices[i+22], mapVertices[i+23]);

            // Blend the normals based on where you are in the triangle
            return glm::normalize(l1 * n0 + l2 * n1 + l3 * n2);
        }
    }
    return glm::vec3(0.0f, 1.0f, 0.0f); // Default to flat ground if not found
}

void Game::loadObject(GameObject& obj, const std::string& path, const std::string& texPath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) return;

    std::vector<float> vertices;
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            // Position
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);
            // UVs
            if (index.texcoord_index >= 0) {
                vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 0]);
                vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 1]);
            } else { vertices.push_back(0.0f); vertices.push_back(0.0f); }
            // Normals
            if (index.normal_index >= 0) {
                vertices.push_back(attrib.normals[3 * index.normal_index + 0]);
                vertices.push_back(attrib.normals[3 * index.normal_index + 1]);
                vertices.push_back(attrib.normals[3 * index.normal_index + 2]);
            } else { vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f); }
        }
    }

    obj.vertexCount = vertices.size() / 8;
    glGenVertexArrays(1, &obj.VAO);
    glGenBuffers(1, &obj.VBO);
    glBindVertexArray(obj.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, obj.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Set attributes (Pos:0, Tex:1, Norm:2) - Same 8-float stride as map
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Load Texture
    glGenTextures(1, &obj.textureID);
    glBindTexture(GL_TEXTURE_2D, obj.textureID);
    sf::Image img;
    if (img.loadFromFile(texPath)) {
        img.flipVertically();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.getSize().x, img.getSize().y, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());
        glGenerateMipmap(GL_TEXTURE_2D);
    }
}

void Game::setupUI() {
    sf::Vector2f winSize = static_cast<sf::Vector2f>(window.getSize());
    float centerX = winSize.x * 0.5f;
    float centerY = winSize.y * 0.5f;

    // 1. Overlay
    pauseOverlay.setSize(winSize);
    pauseOverlay.setPosition({0.f, 0.f});
    pauseOverlay.setFillColor(sf::Color(0, 0, 0, 180));

    // 2. Pause Menu Box (Scales to 25% width, 60% height)
    sf::Vector2f menuSize(winSize.x * 0.25f, winSize.y * 0.6f); 
    pauseMenu.setSize(menuSize);
    pauseMenu.setOrigin(menuSize * 0.5f);
    pauseMenu.setPosition({centerX, centerY});
    pauseMenu.setFillColor(sf::Color(40, 40, 40, 220));

    // 3. Pause Title (Text size is 10% of window height)
    pauseText.setString("PAUSED");
    pauseText.setCharacterSize(static_cast<unsigned int>(winSize.y * 0.1f)); 
    sf::FloatRect textBounds = pauseText.getLocalBounds();
    pauseText.setOrigin({textBounds.position.x + textBounds.size.x / 2.f, 
                        textBounds.position.y + textBounds.size.y / 2.f});
    pauseText.setPosition({centerX, centerY - (menuSize.y * 0.35f)});

    // 4. Buttons (Scales: 15% width, 6% height of window)
    sf::Vector2f buttonSize(winSize.x * 0.15f, winSize.y * 0.06f);
    
    resumeBtn.setSize(buttonSize);
    resumeBtn.setOrigin(buttonSize * 0.5f);
    resumeBtn.setPosition({centerX, centerY - (menuSize.y * 0.05f)});
    resumeBtn.setFillColor(sf::Color(0, 0, 0, 180));
    
    exitBtn.setSize(buttonSize);
    exitBtn.setOrigin(buttonSize * 0.5f);
    exitBtn.setPosition({centerX, centerY + (menuSize.y * 0.1f)});
    exitBtn.setFillColor(sf::Color(0, 0, 0, 180));

    // 5. Scalable Lambda
    auto centerTextInButton = [&](sf::Text& txt, sf::RectangleShape& btn, std::string str) {
        txt.setString(str);
        // Text size is roughly 60% of button height
        txt.setCharacterSize(static_cast<unsigned int>(btn.getSize().y * 0.6f));
        txt.setFillColor(sf::Color::White);

        sf::FloatRect b = txt.getLocalBounds();
        txt.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
        txt.setPosition(btn.getPosition()); 
    };

    centerTextInButton(resumeText, resumeBtn, "RESUME");
    centerTextInButton(exitText, exitBtn, "EXIT");

    // 6. Sensitivity Slider (Scales: 15% width, 0.5% height)
    sf::Vector2f trackSize(winSize.x * 0.15f, winSize.y * 0.005f); 
    sensitivityTrack.setSize(trackSize);
    sensitivityTrack.setOrigin({trackSize.x / 2.f, trackSize.y / 2.f}); 
    // Positioned relative to the menu box bottom
    sensitivityTrack.setPosition({centerX, centerY + (menuSize.y * 0.3f)});

    sf::Vector2f handleSize(winSize.x * 0.01f, winSize.y * 0.03f);
    sensitivityHandle.setSize(handleSize);
    sensitivityHandle.setOrigin(handleSize * 0.5f);

    float minSens = 0.01f;
    float maxSens = 0.2f;
    float t = (sensitivity - minSens) / (maxSens - minSens);
    float trackLeft = sensitivityTrack.getPosition().x - (trackSize.x / 2.f);
    float handleX = trackLeft + (t * trackSize.x);
    sensitivityHandle.setPosition({handleX, sensitivityTrack.getPosition().y});

    // 7. Sensitivity Text (Scales with window height)
    sensitivityText.setString("MOUSE SENSITIVITY");
    sensitivityText.setCharacterSize(static_cast<unsigned int>(winSize.y * 0.025f));
    sensitivityText.setFillColor(sf::Color::White);
    
    sf::FloatRect sBounds = sensitivityText.getLocalBounds();
    sensitivityText.setOrigin({sBounds.position.x + sBounds.size.x / 2.f, 
                               sBounds.position.y + sBounds.size.y / 2.f});
    sensitivityText.setPosition({centerX, sensitivityTrack.getPosition().y - (winSize.y * 0.04f)});
}

Game::~Game() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
}