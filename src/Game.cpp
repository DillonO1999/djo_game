#include "Game.hpp"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- SHADER SOURCE CODE (GLSL) ---
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord; // New attribute

    out vec2 TexCoord;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        TexCoord = aTexCoord;
    }
)glsl";

const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;

    uniform sampler2D texture1; // This represents our image

    void main() {
        FragColor = texture(texture1, TexCoord);
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

void Game::setupResources() {
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    shaderProgram = glCreateProgram();

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

   // 1. Define 8 corners of a cube (x, y, z)
    float vertices[] = {
        // Positions             // Texture (UV)
        // FLOOR (Horizontal)
        -100.0f, 0.0f, -100.0f,    0.0f,  0.0f,
        100.0f, 0.0f, -100.0f,   200.0f,  0.0f,
        100.0f, 0.0f,  100.0f,   200.0f, 200.0f,
        -100.0f, 0.0f,  100.0f,    0.0f, 200.0f,

        // WALL TYPE A (Front/Back - XY Plane)
        -10.0f, 0.0f, 0.0f,      0.0f,  0.0f,
        10.0f, 0.0f, 0.0f,     20.0f,  0.0f,
        10.0f, 4.0f, 0.0f,     20.0f,  4.0f,
        -10.0f, 4.0f, 0.0f,      0.0f,  4.0f,

        // WALL TYPE B (Left/Right - ZY Plane)
        0.0f, 0.0f, -10.0f,     0.0f,  0.0f,
        0.0f, 0.0f,  10.0f,    20.0f,  0.0f,
        0.0f, 4.0f,  10.0f,    20.0f,  4.0f,
        0.0f, 4.0f, -10.0f,     0.0f,  4.0f
    };

    unsigned int indices[] = {
        0, 3, 2, 2, 1, 0,       // Floor (Reversed order to face UP)
        4, 5, 6, 6, 7, 4,       // Wall Front/Back
        8, 9, 10, 10, 11, 8     // Wall Left/Right
    };

    unsigned int EBO; // Element Buffer Object
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Upload Vertex Data
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Upload Index Data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture attribute (Location 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set wrapping/filtering options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // 1. Load the image using SFML
    sf::Image image;
    if (!image.loadFromFile("assets/textures/images.jpeg")) {
        std::cout << "Failed to load texture at assets/textures/images.jpeg" << std::endl;
    } else {
        // 2. OpenGL expects the texture upside down, SFML can fix this easily
        image.flipVertically(); 

        // 3. Get the raw pixel data and dimensions
        const uint8_t* pixelData = image.getPixelsPtr();
        sf::Vector2u size = image.getSize();

        // 4. Upload to the GPU (Note: SFML pixels are usually RGBA)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
}

unsigned int Game::compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    return id;
}

void Game::run() {
    sf::Clock clock;
    
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

        // 2. Use the shader and set Camera/Perspective
        glUseProgram(shaderProgram);
        // --- ADD THESE TWO LINES ---
        glActiveTexture(GL_TEXTURE0); // Activate texture unit 0
        glBindTexture(GL_TEXTURE_2D, textureID); // Bind your texture to it
        glm::mat4 view = camera.getViewMatrix();
        float aspect = window.getSize().x / (float)window.getSize().y;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        unsigned int viewLoc  = glGetUniformLocation(shaderProgram, "view");
        unsigned int projLoc  = glGetUniformLocation(shaderProgram, "projection");

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(VAO);

        // 1. Draw the Floor
        glm::mat4 model = glm::mat4(1.0f); 
        // Move it down to -1.0 so you are standing ON it
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); 
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);

        // --- BUILDING SETUP ---
        // We want a 10x10 building at X=20, Z=20
        float bX = 20.0f;
        float bZ = 20.0f;
        float bWidth = 10.0f;
        float bHeight = 10.0f;
        building.drawBuilding(bX, bZ, bWidth, bHeight, modelLoc);   
        building.drawBuilding(-20.0f, 20.0f, bWidth, bHeight, modelLoc);        

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
        // If crouching, go slow. If standing, go normal.
        camera.speed = camera.isCrouching ? 2.5f : 5.0f;

        glm::vec3 prevPos = camera.position;
        
        // Keyboard Movement (Outside the event loop for smooth motion)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) camera.processKeyboard('W', deltaTime);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) camera.processKeyboard('S', deltaTime);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) camera.processKeyboard('A', deltaTime);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) camera.processKeyboard('D', deltaTime);

        // 1. Define constants
        const float standHeight = 1.5f;
        const float crouchHeight = 0.8f;
        const float floorLevel = 0.0f;
        const float interSpeed = 8.0f; 

        // 2. Determine target height based on the TOGGLE state
        float targetHeight = camera.isCrouching ? crouchHeight : standHeight;
        float targetY = floorLevel + targetHeight;

        // 4. THE SMOOTH HEIGHT SLIDE
        if (camera.isGrounded) {
            // This is the ONLY place position.y is modified while on the floor
            float dy = (targetY - camera.position.y) * interSpeed * deltaTime;
            camera.position.y += dy;
            camera.verticalVelocity = 0.0f; // Kill gravity buildup
        }

        // 4. GRAVITY (Only applies if we fall off a ledge)
        if (!camera.isGrounded) {
            camera.verticalVelocity += camera.gravity * deltaTime;
            camera.position.y += camera.verticalVelocity * deltaTime;
        }

        // 6. DYNAMIC FLOOR CHECK
        // This makes the "Floor" move with your crouch state
        if (camera.position.y <= targetY) {
            if (!camera.isGrounded) {
                camera.position.y = targetY; // Snap ONLY when landing a fall
            }
            camera.isGrounded = true;
            camera.verticalVelocity = 0.0f;
        } else {
            // If we are significantly above our current target, we are in the air
            if (std::abs(camera.position.y - targetY) > 0.01f) {
                camera.isGrounded = false;
            }
        }

        // Define all building positions
        glm::vec2 buildings[] = { {20.0f, 20.0f}, {-20.0f, 20.0f} };
        float pRadius = 0.4f; // Player's collision radius

        for (auto& bPos : buildings) {
            float bX = bPos.x;
            float bZ = bPos.y;
            float half = 5.0f;

            // Boundary Lines
            float left   = bX - half;
            float right  = bX + half;
            float back   = bZ - half;
            float front  = bZ + half;

            // 1. WEST & EAST WALLS (The Side Walls)
            // Check if we are within the Z-length of the walls
            if (camera.position.z > back - pRadius && camera.position.z < front + pRadius) {
                
                // West Wall (Left) - Zone is [left - radius, left + radius]
                if (camera.position.x > left - pRadius && camera.position.x < left + pRadius) {
                    camera.position.x = (prevPos.x <= left - pRadius) ? left - pRadius : left + pRadius;
                }

                // East Wall (Right)
                if (camera.position.x > right - pRadius && camera.position.x < right + pRadius) {
                    camera.position.x = (prevPos.x >= right + pRadius) ? right + pRadius : right - pRadius;
                }
            }

            // 2. NORTH & SOUTH WALLS (Back & Front)
            // Check if we are within the X-width of the walls
            if (camera.position.x > left - pRadius && camera.position.x < right + pRadius) {
                
                // North Wall (Back)
                if (camera.position.z > back - pRadius && camera.position.z < back + pRadius) {
                    camera.position.z = (prevPos.z <= back - pRadius) ? back - pRadius : back + pRadius;
                }

                // South Wall (Front - The Door Wall)
                bool inDoorX = (camera.position.x > bX - 1.2f && camera.position.x < bX + 1.2f);
                if (!inDoorX) {
                    if (camera.position.z > front - pRadius && camera.position.z < front + pRadius) {
                        camera.position.z = (prevPos.z >= front + pRadius) ? front + pRadius : front - pRadius;
                    }
                }
            }
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