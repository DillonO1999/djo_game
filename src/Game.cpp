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
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    shaderProgram = glCreateProgram();

    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "grassTexture"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "rockTexture"), 1);

    // ATTACH FIRST
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);

    // LINK ONCE AFTER ATTACHING
    glLinkProgram(shaderProgram);

    // Check for linking errors (very helpful for debugging segfaults)
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "Shader Linking Error: " << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

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

        glBindVertexArray(mapVAO); // This handles all your pointers automatically

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTextureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "grassTexture"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, rockTextureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "rockTexture"), 1);

        glDrawArrays(GL_TRIANGLES, 0, mapVertices.size() / 8); // Divide by 8 now!

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
        }
    }

    
    if (currentState == GameState::Playing) {
        // --- 1. SET DYNAMIC SPEED ---
        // Change these values to match your new scale
        float baseSpeed = 7.0f; // Was likely 5.0f
        camera.speed = camera.isCrouching ? (baseSpeed * 0.5f) : baseSpeed;

        glm::vec3 prevPos = camera.position;
        
        // Keyboard Movement (Outside the event loop for smooth motion)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) camera.processKeyboard('W', deltaTime);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) camera.processKeyboard('S', deltaTime);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) camera.processKeyboard('A', deltaTime);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) camera.processKeyboard('D', deltaTime);

        // 1. Define constants
        const float standHeight = 1.5f;
        const float crouchHeight = 0.8f;
        const float interSpeed = 8.0f; 

        // 1. Get the height of the Blender mesh at player's X, Z
        // 1. Get the floor height from your OBJ
        float currentMapHeight = getMapHeightAt(camera.position.x, camera.position.z);
        float targetHeight = camera.isCrouching ? crouchHeight : standHeight;
        float targetY = currentMapHeight + targetHeight;

        // 2. Smoothly move toward the target height (Up OR Down)
        // This "Lerp" handles both climbing hills and walking down them.
        float dy = (targetY - camera.position.y) * interSpeed * deltaTime;
        camera.position.y += dy;

        // 3. Grounding check (for jumping logic)
        if (std::abs(camera.position.y - targetY) < 0.1f) {
            camera.isGrounded = true;
            camera.verticalVelocity = 0.0f;
        } else {
            camera.isGrounded = false;
        }

        // 3. Handle Jump Input
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) && camera.isGrounded) {
            camera.verticalVelocity = 5.0f; // Jump force
            camera.isGrounded = false;
        }

        // Mouse Look - CALCULATE EVERY FRAME
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        if (mousePos != windowCenter) { // Only process if the mouse actually moved
            float xoffset = (float)(mousePos.x - windowCenter.x) * sensitivity;
            float yoffset = (float)(windowCenter.y - mousePos.y) * sensitivity; 

            camera.processMouse(xoffset, yoffset);
            sf::Mouse::setPosition(windowCenter, window); // Reset to center
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