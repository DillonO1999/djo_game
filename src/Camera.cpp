#include "Camera.hpp"

glm::mat4 Camera::getViewMatrix() {
    return glm::lookAt(position, position + front, up);
}

void Camera::processKeyboard(char direction, float deltaTime) {
    float velocity = speed * deltaTime;
    
    // Create a "Flat Front" vector (ignore the Y axis for movement)
    glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
    glm::vec3 right = glm::normalize(glm::cross(flatFront, up));

    if (direction == 'W') position += flatFront * velocity;
    if (direction == 'S') position -= flatFront * velocity;
    if (direction == 'A') position -= right * velocity;
    if (direction == 'D') position += right * velocity;

    // DO NOT TOUCH isCrouching HERE! 
    // It is handled by the toggle in Game.cpp
}

void Camera::processMouse(float xoffset, float yoffset) {
    yaw   += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    glm::vec3 f;
    f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    f.y = sin(glm::radians(pitch));
    f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(f);
}