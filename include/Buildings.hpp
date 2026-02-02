#pragma once

#include <glad/glad.h>   // For OpenGL functions (glDrawElements, etc.)
#include <glm/glm.hpp>   // For glm::mat4, glm::vec3
#include <glm/gtc/matrix_transform.hpp> // For glm::translate, glm::scale
#include <glm/gtc/type_ptr.hpp>

class Buildings {

    private:

    public:
        void drawBuilding(float x, float z, float width, float height, unsigned int modelLoc);
};