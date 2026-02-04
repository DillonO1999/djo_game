#ifndef GAME_HPP
#define GAME_HPP

#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include "Camera.hpp"

enum class GameState {
    Playing,
    Paused
};

class Game {
    public:
        Game();
        ~Game(); // Destructor to clean up GPU memory
        void run();

        struct GameObject {
            unsigned int VAO, VBO;
            int vertexCount;
            unsigned int textureID;
            
            glm::vec3 position;
            glm::vec3 rotation; // in degrees
            glm::vec3 scale;
            glm::vec3 groundNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            bool isTree = false;

            GameObject() : VAO(0), VBO(0), vertexCount(0), textureID(0), 
                        position(0.0f), rotation(0.0f), scale(1.0f) {}

            glm::mat4 getModelMatrix() {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), position);

                // Calculate Slope Alignment
                glm::vec3 worldUp(0, 1, 0);
                if (glm::length(groundNormal - worldUp) > 0.001f) {
                    float angle = acos(glm::dot(worldUp, groundNormal));
                    glm::vec3 axis = glm::normalize(glm::cross(worldUp, groundNormal));
                    model = glm::rotate(model, angle, axis);
                }

                // Apply the local Y-rotation (turning the fence)
                model = glm::rotate(model, glm::radians(rotation.y), worldUp);
                model = glm::scale(model, scale);
                
                return model;
            }
        };

        void loadObject(GameObject& obj, const std::string& path, const std::string& texPath);
        void insertObject(GameObject& object, std::string object_file, std::string texture, glm::vec2 loc, glm::vec3 rot, glm::vec3 scale);

        bool isCreativeMode = false; // Toggle this with a key

        // ADD THESE LINES:
        GameObject GrassTemplate;      // This stores the mesh and texture info
        unsigned int numGrassInstances; // This stores how many to draw
        unsigned int grassInstanceVBO;  // This stores the buffer ID for the positions

    private:
        void processEvents(float deltaTime);
        void setupResources();
        unsigned int compileShader(unsigned int type, const char* source);

        sf::RenderWindow window; // We use sf::Window instead of RenderWindow for pure OpenGL
        unsigned int VAO, VBO, shaderProgram;

        Camera camera;
        sf::Vector2i lastMousePos;
        bool firstMouse = true;

        sf::Vector2i windowCenter;
        bool isPaused = false;

        GameState currentState = GameState::Playing;
    
        // SFML UI elements
        sf::RectangleShape pauseOverlay;
        sf::RectangleShape pauseMenu;
        sf::Font font;
        sf::Text pauseText;
        sf::RectangleShape resumeBtn;
        sf::Text resumeText;
        sf::RectangleShape exitBtn;
        sf::Text exitText;
        sf::Text sensitivityText;


        unsigned int textureID;

        float sensitivity = 0.1f;
        sf::RectangleShape sensitivityTrack;
        sf::RectangleShape sensitivityHandle;
        bool draggingSlider = false;

        std::vector<float> mapVertices;
        unsigned int mapVAO, mapVBO;

        unsigned int grassTextureID;
        unsigned int rockTextureID;
        // Keep your old textureID if you still need it, or replace it.

        void setupUI();
        void loadMap(const std::string& path);
        float getMapHeightAt(float x, float z);
        glm::vec3 getMapNormalAt(float x, float z);

        
        std::vector<GameObject> sceneObjects;

        // Inside Game class members
        unsigned int objectShaderProgram;
};

#endif