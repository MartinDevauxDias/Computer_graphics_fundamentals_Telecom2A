#include "window.h"
#include "scene.h" // Include scene.h for the processInput function
#include <iostream>

Window::Window(unsigned int w, unsigned int h, const char* title) : width(w), height(h) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    ptr = glfwCreateWindow(width, height, title, NULL, NULL);
    if (ptr == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(ptr);
    glfwSetWindowUserPointer(ptr, this); // Link this instance to the GLFW window

    // Set callbacks
    glfwSetFramebufferSizeCallback(ptr, framebuffer_size_callback);
    glfwSetScrollCallback(ptr, scroll_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
    }
}

Window::~Window() {
    glfwDestroyWindow(ptr);
    glfwTerminate();
}

bool Window::shouldClose() {
    return glfwWindowShouldClose(ptr);
}

void Window::update() {
    glfwSwapBuffers(ptr);
    glfwPollEvents();
}

void Window::processInput(Scene& currentScene) {
    if (glfwGetKey(ptr, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(ptr, true);

    // Visualization Toggles
    if (glfwGetKey(ptr, GLFW_KEY_G) == GLFW_PRESS) wireframeMode = true;
    if (glfwGetKey(ptr, GLFW_KEY_F) == GLFW_PRESS) wireframeMode = false;
    if (glfwGetKey(ptr, GLFW_KEY_R) == GLFW_PRESS) raytracingMode = true;
    if (glfwGetKey(ptr, GLFW_KEY_T) == GLFW_PRESS) raytracingMode = false;

    // Camera movement
    if (glfwGetKey(ptr, GLFW_KEY_W) == GLFW_PRESS) cameraPhi -= rotationSpeed;
    if (glfwGetKey(ptr, GLFW_KEY_S) == GLFW_PRESS) cameraPhi += rotationSpeed;
    if (glfwGetKey(ptr, GLFW_KEY_A) == GLFW_PRESS) cameraTheta += rotationSpeed;
    if (glfwGetKey(ptr, GLFW_KEY_D) == GLFW_PRESS) cameraTheta -= rotationSpeed;

    // Delegate scene-specific input
    currentScene.processInput(*this);
}

void Window::updateCamera() {
    cameraPhi = glm::clamp(cameraPhi, 0.1f, (float)M_PI - 0.1f);
    cameraPos.x = cameraDistance * sin(cameraPhi) * cos(cameraTheta);
    cameraPos.y = cameraDistance * cos(cameraPhi);
    cameraPos.z = cameraDistance * sin(cameraPhi) * sin(cameraTheta);
}

glm::mat4 Window::getViewMatrix() {
    return glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Window::getProjectionMatrix() {
    return glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
}

// --- Static Callbacks ---
void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win) {
        win->width = width;
        win->height = height;
        glViewport(0, 0, width, height);
    }
}

void Window::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win) {
        win->cameraDistance -= static_cast<float>(yoffset) * win->zoomSpeed;
        win->cameraDistance = glm::clamp(win->cameraDistance, 1.0f, 30.0f);
    }
}