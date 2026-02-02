#include "Buildings.hpp"

void Buildings::drawBuilding(float x, float z, float width, float height, unsigned int modelLoc) {
    float half = width / 2.0f;
    float scaleW = width / 20.0f; 
    float scaleH = height / 4.0f;

    // --- NORTH WALL (Solid - Type A) ---
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z - half));
    model = glm::scale(model, glm::vec3(scaleW, scaleH, 1.0f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(6 * sizeof(unsigned int)));

    // --- SOUTH WALL (Doorway - Type A) ---
    float segmentScale = scaleW * 0.4f;
    // Left Segment
    model = glm::translate(glm::mat4(1.0f), glm::vec3(x - (half * 0.6f), 0.0f, z + half));
    model = glm::scale(model, glm::vec3(segmentScale, scaleH, 1.0f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(6 * sizeof(unsigned int)));
    // Right Segment
    model = glm::translate(glm::mat4(1.0f), glm::vec3(x + (half * 0.6f), 0.0f, z + half));
    model = glm::scale(model, glm::vec3(segmentScale, scaleH, 1.0f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(6 * sizeof(unsigned int)));

    // --- WEST WALL (Type B - Note the index offset change to 12) ---
    model = glm::translate(glm::mat4(1.0f), glm::vec3(x - half, 0.0f, z));
    model = glm::scale(model, glm::vec3(1.0f, scaleH, scaleW)); // Scale Z for side walls
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(12 * sizeof(unsigned int)));

    // --- EAST WALL (Type B) ---
    model = glm::translate(glm::mat4(1.0f), glm::vec3(x + half, 0.0f, z));
    model = glm::scale(model, glm::vec3(1.0f, scaleH, scaleW));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(12 * sizeof(unsigned int)));
}