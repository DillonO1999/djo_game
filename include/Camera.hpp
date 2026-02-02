#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
    public:
        glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
        glm::vec3 front    = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up       = glm::vec3(0.0f, 1.0f, 0.0f);

        float yaw   = -90.0f; // Left/Right rotation
        float pitch =  0.0f;  // Up/Down rotation
        float speed =  5.5f;


        float verticalVelocity = 0.0f;
        bool isGrounded = true;
        float gravity = -9.8f;

        glm::mat4 getViewMatrix();

        void processKeyboard(char direction, float deltaTime);

        void processMouse(float xoffset, float yoffset);

        bool isCrouching = false;

    private:
        void updateCameraVectors();

};