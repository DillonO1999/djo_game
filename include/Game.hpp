#pragma once

#include "Player.hpp"
#include "Physics.hpp" // Make sure this is included!
#include "raylib.h"
#include <vector>
#include <string>

enum class GameState {
    Playing,
    Paused
};

class Game {
    public:
        Game();
        ~Game(); 
        void run();

        struct GameObject {
            Model model;        
            Texture2D texture;  
            Vector3 position;
            Vector3 rotation;   
            Vector3 scale;
            Vector3 groundNormal;
            bool isTree = false;
        };

    private:   
        // 1. ADD THE PLAYER OBJECT HERE
        Player player; 

        // Camera & State
        float cameraYaw = -90.0f;
        float cameraPitch = 0.0f;
        float sliderValue = 0.25f;
        float sensitivity = 0.0575f;
        bool draggingSlider = false;

        void processEvents(float deltaTime);
        void setupResources();
        void setupUI();
        
        Camera3D camera;        
        GameState currentState;

        Rectangle pauseMenuRect, resumeBtnRect, exitBtnRect, sliderTrackRect, sliderHandleRect;

        // Assets
        Model mapModel;         
        Texture2D grassTexture;
        Texture2D rockTexture;
        
        // Note: You can eventually remove playerModel/playerAnims from here 
        // since the Player class handles them now!
        Model vehicleModel;
        Vector3 vehiclePos;

        std::vector<GameObject> sceneObjects;
        Shader terrainShader;

        Ball gameBall; 
};