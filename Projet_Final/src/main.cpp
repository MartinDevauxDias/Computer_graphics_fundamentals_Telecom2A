#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "shader.h"
#include "object.h"
#include "rigidsolver.h"
#include "scene.h"
#include "renderer.h"
#include "window.h"

#include <iostream>
#include <cmath>
#include <vector>
#include <omp.h>

// Forward declarations
class RigidSolver;

// Function declarations
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
GLFWwindow *initializeGLFW();

// Window settings 
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float timeAccumulator = 0.0f;

int main()
{
    Window window(SCR_WIDTH, SCR_HEIGHT, "Final Project");
    { 
        Renderer renderer(window.width, window.height);
        Scene* currentScene = new PhysicsStackScene();

        // Move this declaration outside the loop
        double totalPhysTime = 0.0; 

        // Render loop
        while (!window.shouldClose())
        {
            // Calculate delta time
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            window.processInput(*currentScene, deltaTime);

            // Toggle Scene logic with keyboard keys '1' and '2'
            if (glfwGetKey(window.ptr, GLFW_KEY_1) == GLFW_PRESS) {
                if (!dynamic_cast<PhysicsStackScene*>(currentScene)) {
                    delete currentScene;
                    currentScene = new PhysicsStackScene();
                }
            }
            if (glfwGetKey(window.ptr, GLFW_KEY_2) == GLFW_PRESS) {
                if (!dynamic_cast<RayTracingScene*>(currentScene)) {
                    delete currentScene;
                    currentScene = new RayTracingScene();
                }
            }

            // Physics update loop
            float fixedTimeStep = 0.01f; 
            timeAccumulator += std::min(deltaTime, 0.1f);
            while (timeAccumulator >= fixedTimeStep) {
                double physStart = glfwGetTime();
                currentScene->step(fixedTimeStep);
                double physEnd = glfwGetTime();
                totalPhysTime += (physEnd - physStart); // Accumulate over multiple frames

                timeAccumulator -= fixedTimeStep;
            }

            double rtPrepTime = renderer.render(*currentScene, window.getViewMatrix(), window.getProjectionMatrix(), window.cameraPos, window.raytracingMode, window.wireframeMode);

            window.update();
            
            static int frameCount = 0;
            static double lastTime = 0.0;
            frameCount++;

            if (currentFrame - lastTime >= 1.0) { // Update title every second
                char title[256];
                sprintf(title, "Raytracer | FPS: %d | Phys: %.2fms | RTPrep: %.2fms", 
                        frameCount, totalPhysTime * 1000.0, rtPrepTime * 1000.0);
                glfwSetWindowTitle(window.ptr, title);
                frameCount = 0;
                lastTime = currentFrame;
                totalPhysTime = 0.0; // Reset the accumulator for the next second
            }
        }

        delete currentScene;
    }

    return 0;
}
