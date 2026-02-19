#include "window.h"
#include "scene.h" 
#include <iostream>
#include <vector>
#include <string>
#include <ctime>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../external/glfw/deps/stb_image_write.h"

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

    glfwGetWindowPos(ptr, &windowedX, &windowedY);
    windowedWidth = width;
    windowedHeight = height;

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

    static bool f11Pressed = false;
    if (glfwGetKey(ptr, GLFW_KEY_F11) == GLFW_PRESS)
    {
        if (!f11Pressed)
        {
            toggleFullscreen();
            f11Pressed = true;
        }
    }
    else
    {
        f11Pressed = false;
    }

    static bool pPressed = false;
    if (glfwGetKey(ptr, GLFW_KEY_P) == GLFW_PRESS)
    {
        if (!pPressed)
        {
            saveScreenshot();
            pPressed = true;
        }
    }
    else
    {
        pPressed = false;
    }

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
void Window::toggleFullscreen()
{
    isFullscreen = !isFullscreen;
    if (isFullscreen)
    {
        glfwGetWindowPos(ptr, &windowedX, &windowedY);
        glfwGetWindowSize(ptr, &windowedWidth, &windowedHeight);

        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(ptr, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    else
    {
        glfwSetWindowMonitor(ptr, NULL, windowedX, windowedY, windowedWidth, windowedHeight, 0);
    }
}

void Window::saveScreenshot()
{
    std::vector<unsigned char> pixels(3 * width * height);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Flip vertically (OpenGL starts from bottom-left)
    std::vector<unsigned char> flippedPixels(3 * width * height);
    for (int y = 0; y < (int)height; y++)
    {
        for (int x = 0; x < (int)width; x++)
        {
            flippedPixels[3 * (x + (height - 1 - y) * width) + 0] = pixels[3 * (x + y * width) + 0];
            flippedPixels[3 * (x + (height - 1 - y) * width) + 1] = pixels[3 * (x + y * width) + 1];
            flippedPixels[3 * (x + (height - 1 - y) * width) + 2] = pixels[3 * (x + y * width) + 2];
        }
    }

    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buf[128];
    strftime(buf, sizeof(buf), "screenshots/screenshot_%Y%m%d_%H%M%S.png", ltm);

    if (stbi_write_png(buf, width, height, 3, flippedPixels.data(), width * 3))
    {
        std::cout << "Screenshot saved to " << buf << std::endl;
    }
    else
    {
        std::cerr << "Failed to save screenshot!" << std::endl;
    }
}

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