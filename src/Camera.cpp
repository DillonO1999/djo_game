#include "Camera.hpp"

glm::mat4 Camera::getViewMatrix() {
    return glm::lookAt(position, position + front, up);
}

void Camera::processKeyboard(char direction, float deltaTime, bool isCreative) {
    float velocity = speed * deltaTime;
    
    // Calculate directions
    glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
    glm::vec3 right = glm::normalize(glm::cross(flatFront, up));

    // Determine which forward vector to use
    // front = true 3D direction (includes looking up/down)
    // flatFront = walking on a plane
    glm::vec3 moveForward = isCreative ? front : flatFront;

    if (direction == 'W') position += moveForward * velocity;
    if (direction == 'S') position -= moveForward * velocity;
    if (direction == 'A') position -= right * velocity;
    if (direction == 'D') position += right * velocity;
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