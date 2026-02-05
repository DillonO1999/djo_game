#pragma once

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
            Model model;        // Replaces VAO/VBO/vertexCount
            Texture2D texture;  // Replaces textureID
            
            Vector3 position;
            Vector3 rotation;   // Rotation in degrees (Euler)
            Vector3 scale;
            Vector3 groundNormal;
            
            bool isTree = false;

            // Note: Raylib handles matrix math internally during DrawModelEx,
            // so we don't need a complex getModelMatrix() anymore!
        };

        bool isCreativeMode = false;

    private:   
        // Physics variables (the ones you just fixed)
        bool isCrouching = false;
        bool isSprinting = false;
        bool isGrounded = false;
        float verticalVelocity = 0.0f;
        float speedMultiplier = 1.0f;
        float currentEyeHeight = 1.5f;

        // View variables
        float cameraYaw = -90.0f;
        float cameraPitch = 0.0f;

        float sliderValue = 0.25f; // Visual 0.0 to 1.0
        float sensitivity = 0.0575f; // Math value (mapped from 0.25)
        bool draggingSlider = false;

        void processEvents(float deltaTime);
        void setupResources();
        void setupUI();
        
        // Logic Helpers
        void loadObject(GameObject& obj, const std::string& path, const std::string& texPath);
        float getMapHeightAt(float x, float z);
        Vector3 getMapNormalAt(float x, float z);

        // Raylib Window & Camera
        Camera3D camera;        // Replaces your custom Camera class
        GameState currentState;

        Rectangle pauseMenuRect, resumeBtnRect, exitBtnRect, sliderTrackRect, sliderHandleRect;

        // Raylib doesn't need "Shape" objects stored as variables for UI.
        // We usually define UI sizes and draw them immediately in run().
        // However, we can keep some variables for slider logic:
        

        // Assets
        Model mapModel;         // The main terrain
        Texture2D grassTexture;
        Texture2D rockTexture;
        
        std::vector<GameObject> sceneObjects;
        
        // For Custom Terrain Shading (Slope Blending)
        Shader terrainShader;
};