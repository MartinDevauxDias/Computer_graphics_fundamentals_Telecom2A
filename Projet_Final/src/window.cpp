#include "window.h"
#include "scene.h" // Include scene.h for the processInput function
#include <iostream>

Window::Window(unsigned int w, unsigned int h, const char *title) : width(w), height(h)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    ptr = glfwCreateWindow(width, height, title, NULL, NULL);
    if (ptr == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(ptr);
    glfwSetWindowUserPointer(ptr, this); // Link this instance to the GLFW window

    glfwSetInputMode(ptr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Set callbacks
    glfwSetFramebufferSizeCallback(ptr, framebuffer_size_callback);
    glfwSetCursorPosCallback(ptr, mouse_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
    }
}

Window::~Window()
{
    glfwDestroyWindow(ptr);
    glfwTerminate();
}

bool Window::shouldClose()
{
    return glfwWindowShouldClose(ptr);
}

void Window::update()
{
    glfwSwapBuffers(ptr);
    glfwPollEvents();
}

void Window::processInput(Scene &currentScene, float deltaTime)
{
    if (glfwGetKey(ptr, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(ptr, true);

    // Visualization Toggles
    if (glfwGetKey(ptr, GLFW_KEY_G) == GLFW_PRESS)
        wireframeMode = true;
    if (glfwGetKey(ptr, GLFW_KEY_F) == GLFW_PRESS)
        wireframeMode = false;
    if (glfwGetKey(ptr, GLFW_KEY_R) == GLFW_PRESS)
        raytracingMode = true;
    if (glfwGetKey(ptr, GLFW_KEY_T) == GLFW_PRESS)
        raytracingMode = false;

    // Camera movement
    float speed = movementSpeed * deltaTime;
    glm::vec3 front_horizontal = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 right_horizontal = glm::normalize(glm::cross(front_horizontal, glm::vec3(0.0f, 1.0f, 0.0f)));

    if (glfwGetKey(ptr, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += speed * front_horizontal;
    if (glfwGetKey(ptr, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= speed * front_horizontal;
    if (glfwGetKey(ptr, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= speed * right_horizontal;
    if (glfwGetKey(ptr, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += speed * right_horizontal;

    if (glfwGetKey(ptr, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos.y += speed;
    if (glfwGetKey(ptr, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        cameraPos.y -= speed;

    // Delegate scene-specific input
    currentScene.processInput(ptr, cameraPos, this->getViewMatrix(), this->getProjectionMatrix());
}

glm::mat4 Window::getViewMatrix()
{
    return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
}

glm::mat4 Window::getProjectionMatrix()
{
    return glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
}

// --- Static Callbacks ---
void Window::framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    Window *win = static_cast<Window *>(glfwGetWindowUserPointer(window));
    if (win)
    {
        win->width = width;
        win->height = height;
        glViewport(0, 0, width, height);
    }
}

void Window::mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    Window *win = static_cast<Window *>(glfwGetWindowUserPointer(window));
    if (win)
    {
        if (win->firstMouse)
        {
            win->lastX = xpos;
            win->lastY = ypos;
            win->firstMouse = false;
        }

        float xoffset = xpos - win->lastX;
        float yoffset = win->lastY - ypos; // Reversed since y-coordinates go from bottom to top
        win->lastX = xpos;
        win->lastY = ypos;

        xoffset *= win->mouseSensitivity;
        yoffset *= win->mouseSensitivity;

        win->yaw += xoffset;
        win->pitch += yoffset;

        // Constrain pitch to avoid flipping
        if (win->pitch > 89.0f)
            win->pitch = 89.0f;
        if (win->pitch < -89.0f)
            win->pitch = -89.0f;

        // Recalculate the cameraFront vector
        glm::vec3 front;
        front.x = cos(glm::radians(win->yaw)) * cos(glm::radians(win->pitch));
        front.y = sin(glm::radians(win->pitch));
        front.z = sin(glm::radians(win->yaw)) * cos(glm::radians(win->pitch));
        win->cameraFront = glm::normalize(front);
    }
}