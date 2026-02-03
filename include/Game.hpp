#ifndef GAME_HPP
#define GAME_HPP

#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include "Camera.hpp"
#include "Buildings.hpp"

enum class GameState {
    Playing,
    Paused
};

class Game {
    public:
        Game();
        ~Game(); // Destructor to clean up GPU memory
        void run();

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

        Buildings building;

        std::vector<float> mapVertices;
        unsigned int mapVAO, mapVBO;

        unsigned int grassTextureID;
        unsigned int rockTextureID;
        // Keep your old textureID if you still need it, or replace it.

        void setupUI();
        void loadMap(const std::string& path);
        float getMapHeightAt(float x, float z);
};

#endif