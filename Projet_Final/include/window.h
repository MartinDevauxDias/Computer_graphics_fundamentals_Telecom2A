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
    glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 10.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;

    // Mouse state
    float lastX;
    float lastY;
    bool firstMouse = true;

    // Input state
    bool wireframeMode = false;
    bool raytracingMode = false;
    bool leftMousePressed = false;
    bool rightMousePressed = false;
    bool isFullscreen = false;

    Window(unsigned int w, unsigned int h, const char* title);
    ~Window();

    bool shouldClose();
    void update(); // Swaps buffers and polls events
    void processInput(Scene& currentScene, float deltaTime);
    void toggleFullscreen();

    glm::mat4 getViewMatrix();
    glm::mat4 getProjectionMatrix();

private:
    float movementSpeed = 5.0f;
    float mouseSensitivity = 0.1f;

    int windowedX, windowedY, windowedWidth, windowedHeight;

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
};

#endif // WINDOW_H