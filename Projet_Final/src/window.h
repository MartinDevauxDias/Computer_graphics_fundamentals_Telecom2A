#ifndef WINDOW_H
#define WINDOW_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Forward declare to avoid circular dependencies
class Scene;

class Window {
public:
    GLFWwindow* ptr = nullptr;
    unsigned int width = 800;
    unsigned int height = 800;

    // Camera state
    float cameraDistance = 10.0f;
    float cameraTheta = 0.0f;
    float cameraPhi = 1.57f;
    glm::vec3 cameraPos;

    // Input state
    bool wireframeMode = false;
    bool raytracingMode = false;
    bool leftMousePressed = false;
    bool rightMousePressed = false;

    Window(unsigned int w, unsigned int h, const char* title);
    ~Window();

    bool shouldClose();
    void update(); // Swaps buffers and polls events
    void processInput(Scene& currentScene);
    void updateCamera();

    glm::mat4 getViewMatrix();
    glm::mat4 getProjectionMatrix();

private:
    float rotationSpeed = 0.05f;
    float zoomSpeed = 0.5f;

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
};

#endif // WINDOW_H