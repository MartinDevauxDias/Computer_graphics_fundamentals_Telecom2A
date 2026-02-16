#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "shader.h"
#include "object.h"
#include "icosahedron.h"
#include "rigidsolver.h"

#include <iostream>
#include <cmath>
#include <vector>
#include <omp.h>

// Forward declarations
class RigidSolver;

// Function declarations
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window, RigidSolver& solver, std::vector<Object*>& objects, Mesh* sphereMesh, Material* sphereMat);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
GLFWwindow *initializeGLFW();

// Window settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;

// Camera settings
float cameraDistance = 10.0f;
float cameraTheta = 0.0f;      // Horizontal angle (around Y axis)
float cameraPhi = M_PI / 2.0f; // Vertical angle (from Y axis)
float rotationSpeed = 0.05f;
float zoomSpeed = 0.5f;

glm::vec3 cameraPos;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float timeAccumulator = 0.0f;

bool wireframeMode = false;
bool raytracingMode = false;

struct GPUTriangle {
    glm::vec4 v0;
    glm::vec4 v1;
    glm::vec4 v2;
    glm::vec4 color;
};

struct GPUObject {
    glm::vec4 bmin; // xyz: min, w: reflectivity
    glm::vec4 bmax; // xyz: max, w: triangle start index
    int triangleCount;
    int padding[3];
};

int main()
{
	GLFWwindow *window = initializeGLFW();
	if (!window)
		return -1;

    { // Scope to ensure destructors run before context destruction
        glEnable(GL_DEPTH_TEST);

        Shader shaderProgram("src/vertex_shader.glsl", "src/fragment_shader.glsl");
        Shader computeShader("src/raytracing_compute.glsl");
        Shader screenShader("src/screen_vertex.glsl", "src/screen_fragment.glsl");

        // Texture for compute shader output
        unsigned int textureOutput;
        glGenTextures(1, &textureOutput);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureOutput);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glBindImageTexture(0, textureOutput, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        // Full-screen quad for displaying compute result
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        unsigned int quadVAO, quadVBO;
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        // SSBO for triangles
        unsigned int triangleSSBO;
        glGenBuffers(1, &triangleSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangleSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangleSSBO);

        // SSBO for objects (AABBs and offsets)
        unsigned int objectSSBO;
        glGenBuffers(1, &objectSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, objectSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, objectSSBO);

        // Create shared Box assets
        Mesh* boxMesh = new Mesh({}, {});
        boxMesh->addCube(1.0f);
        boxMesh->subdivideLinear(); 
        boxMesh->subdivideLinear();
        Material boxMaterial;

        // Create shared Sphere assets for the "bolt"
        Mesh* sphereMesh = Icosahedron::createIcosphere(1.0f, 2); // Reduced from 3 to 2 for better performance
        Material sphereMaterial;

        std::vector<Object*> boxes;
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                for (int z = 0; z < 3; ++z) {
                    Object* box = new Object(boxMesh, &boxMaterial);
                    // Center the 3x3 stack. Each box is 1x1x1.
                    // Floor is at -2.0. So bottom boxes (y=0) should be at -1.5.
                    box->setPosition(glm::vec3(x - 1.0f, y - 1.5f, z - 1.0f));
                    box->setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
                    box->setAsBox(1.0f, 1.0f, 1.0f, 5.0f); // Lighter density for the stack
                    box->fixedObject = false;
                    box->restitution = 0.1f;
                    boxes.push_back(box);
                }
            }
        }

        // Create ground using new procedural plan
        Mesh* groundMesh = new Mesh({}, {});
        groundMesh->addPlan(15.0f);
        Material groundMaterial;
        Object ground(groundMesh, &groundMaterial);
        ground.setPosition(glm::vec3(0.0f, -2.0f, 0.0f));
        ground.fixedObject = true;

        // Physics Solver setup
        RigidSolver solver(glm::vec3(0.0f, -9.81f, 0.0f), -2.0f);
        for (auto* box : boxes) {
            solver.addObject(box);
        }

        // Render loop
        while (!glfwWindowShouldClose(window))
        {
            // Calculate delta time
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            processInput(window, solver, boxes, sphereMesh, &sphereMaterial);

            // Updated Orbital Camera Logic (Manual)
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPhi -= rotationSpeed;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPhi += rotationSpeed;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraTheta += rotationSpeed;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraTheta -= rotationSpeed;

            cameraPhi = glm::clamp(cameraPhi, 0.1f, (float)M_PI - 0.1f);
            cameraPos.x = cameraDistance * sin(cameraPhi) * cos(cameraTheta);
            cameraPos.y = cameraDistance * cos(cameraPhi);
            cameraPos.z = cameraDistance * sin(cameraPhi) * sin(cameraTheta);

            glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

            double physStart = glfwGetTime();
            // Update physics using sub-stepping for better stability
            // Increased to 400Hz (0.0025s) for better collision response with fast/big objects
            float fixedTimeStep = 0.0025f; 
            timeAccumulator += std::min(deltaTime, 0.1f); // Cap to 100ms to prevent death spiral
            while (timeAccumulator >= fixedTimeStep) {
                solver.step(fixedTimeStep);
                timeAccumulator -= fixedTimeStep;
            }
            double physEnd = glfwGetTime();

            // Light properties
            glm::vec3 lightPos(3.0f, 5.0f, 2.0f);
            glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

            // ======= Render =======
            glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            double rtPrepTime = 0.0;
            if (raytracingMode) {
                double prepStart = glfwGetTime();
                
                // 1. Calculate Offsets (Serial but fast)
                std::vector<size_t> offsets(boxes.size() + 1);
                offsets[0] = ground.mesh->indices.size() / 3;
                for(size_t i = 0; i < boxes.size(); ++i) {
                    offsets[i+1] = offsets[i] + boxes[i]->mesh->indices.size() / 3;
                }

                size_t totalTris = offsets.back();
                std::vector<GPUTriangle> gpuTriangles(totalTris);
                std::vector<GPUObject> gpuObjects(boxes.size() + 1);
                
                // 2. Process Ground
                {
                    glm::mat4 groundModel = ground.getModelMatrix();
                    glm::vec3 groundColor(0.3f, 0.5f, 0.3f);
                    float groundReflectivity = 0.6f;
                    glm::vec3 bmin(1e30f), bmax(-1e30f);

                    for (size_t i = 0; i < ground.mesh->indices.size(); i += 3) {
                        GPUTriangle& tri = gpuTriangles[i/3];
                        tri.v0 = groundModel * glm::vec4(ground.mesh->vertices[ground.mesh->indices[i]].position, 1.0f);
                        tri.v1 = groundModel * glm::vec4(ground.mesh->vertices[ground.mesh->indices[i+1]].position, 1.0f);
                        tri.v2 = groundModel * glm::vec4(ground.mesh->vertices[ground.mesh->indices[i+2]].position, 1.0f);
                        tri.color = glm::vec4(groundColor, groundReflectivity);

                        bmin = glm::min(bmin, glm::vec3(tri.v0)); bmin = glm::min(bmin, glm::vec3(tri.v1)); bmin = glm::min(bmin, glm::vec3(tri.v2));
                        bmax = glm::max(bmax, glm::vec3(tri.v0)); bmax = glm::max(bmax, glm::vec3(tri.v1)); bmax = glm::max(bmax, glm::vec3(tri.v2));
                    }
                    gpuObjects[0] = { glm::vec4(bmin, groundReflectivity), glm::vec4(bmax, 0.0f), (int)(ground.mesh->indices.size()/3) };
                }

                // 3. Process all dynamic objects in parallel
                #pragma omp parallel for
                for (int i = 0; i < (int)boxes.size(); ++i) {
                    glm::vec4 color;
                    float reflect = 0.0f;
                    if (boxes[i]->collisionRadius > 0.0f) {
                        color = glm::vec4(1.0f, 0.2f, 0.2f, 0.0f); // Bolts
                    } else {
                        float r = 0.4f + 0.4f * sin(i * 0.5f);
                        float g = 0.4f + 0.4f * cos(i * 0.3f);
                        float b = 0.6f + 0.2f * sin(i * 0.8f);
                        color = glm::vec4(r, g, b, 0.0f); 
                        reflect = 0.2f; // Boxes are slightly reflective
                    }
                    
                    glm::mat4 model = boxes[i]->getModelMatrix();
                    size_t startIdx = offsets[i];
                    size_t numTris = boxes[i]->mesh->indices.size() / 3;
                    
                    glm::vec3 bmin(1e30f), bmax(-1e30f);
                    for (size_t j = 0; j < numTris; ++j) {
                        GPUTriangle& tri = gpuTriangles[startIdx + j];
                        tri.v0 = model * glm::vec4(boxes[i]->mesh->vertices[boxes[i]->mesh->indices[j*3]].position, 1.0f);
                        tri.v1 = model * glm::vec4(boxes[i]->mesh->vertices[boxes[i]->mesh->indices[j*3+1]].position, 1.0f);
                        tri.v2 = model * glm::vec4(boxes[i]->mesh->vertices[boxes[i]->mesh->indices[j*3+2]].position, 1.0f);
                        tri.color = glm::vec4(glm::vec3(color), reflect);

                        bmin = glm::min(bmin, glm::vec3(tri.v0)); bmin = glm::min(bmin, glm::vec3(tri.v1)); bmin = glm::min(bmin, glm::vec3(tri.v2));
                        bmax = glm::max(bmax, glm::vec3(tri.v0)); bmax = glm::max(bmax, glm::vec3(tri.v1)); bmax = glm::max(bmax, glm::vec3(tri.v2));
                    }
                    gpuObjects[i+1] = { glm::vec4(bmin, reflect), glm::vec4(bmax, (float)startIdx), (int)numTris };
                }

                // Update SSBOs
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangleSSBO);
                glBufferData(GL_SHADER_STORAGE_BUFFER, gpuTriangles.size() * sizeof(GPUTriangle), gpuTriangles.data(), GL_DYNAMIC_DRAW);
                
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, objectSSBO);
                glBufferData(GL_SHADER_STORAGE_BUFFER, gpuObjects.size() * sizeof(GPUObject), gpuObjects.data(), GL_DYNAMIC_DRAW);

                rtPrepTime = glfwGetTime() - prepStart;
                
                // DISPATCH COMPUTE SHADER
                computeShader.use();
                computeShader.set("objectCount", (int)gpuObjects.size());

                // Pass camera data
                computeShader.set("invView", glm::inverse(view));
                computeShader.set("invProjection", glm::inverse(projection));
                computeShader.set("cameraPos", cameraPos);

                glDispatchCompute((unsigned int)SCR_WIDTH / 16, (unsigned int)SCR_HEIGHT / 16, 1);
                
                // make sure writing to image has finished before read
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                // RENDER SCREEN QUAD
                screenShader.use();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, textureOutput);
                screenShader.set("screenTexture", 0);
                glBindVertexArray(quadVAO);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                glBindVertexArray(0);
            } else {
                if (wireframeMode)
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                else
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                shaderProgram.use();
                
                // Light properties
                shaderProgram.set("lightPos", lightPos);
                shaderProgram.set("lightColor", lightColor);
                shaderProgram.set("viewPos", cameraPos);
                
                // Transformation matrices
                shaderProgram.set("view", view);
                shaderProgram.set("projection", projection);

                // Draw ground
                shaderProgram.set("objectColor", 0.3f, 0.5f, 0.3f);  // Green ground
                ground.draw(shaderProgram);

                // Draw all boxes and spheres
                for (auto* obj : boxes) {
                    if (obj->collisionRadius > 0.0f) {
                        shaderProgram.set("objectColor", 1.0f, 0.2f, 0.2f); // Red spheres for bolts
                    } else {
                        shaderProgram.set("objectColor", 0.4f, 0.4f, 0.8f); // Blue boxes
                    }
                    obj->draw(shaderProgram);
                }
            }

            glfwSwapBuffers(window);
            glfwPollEvents();

            // Display timings in window title
            static int frameCount = 0;
            static double lastTime = 0.0;
            frameCount++;
            if (currentFrame - lastTime >= 1.0) { // Update every second
                char title[256];
                double avgFrameTime = 1000.0 / (double)frameCount;
                sprintf(title, "Raytracer | FPS: %d | Phys: %.2fms | RTPrep: %.2fms | CPU Total: %.2fms", 
                        frameCount, (physEnd - physStart)*1000.0, rtPrepTime*1000.0, (physEnd - physStart + rtPrepTime)*1000.0);
                glfwSetWindowTitle(window, title);
                frameCount = 0;
                lastTime = currentFrame;
            }
        }

        // Cleanup resources
        glDeleteTextures(1, &textureOutput);
        glDeleteVertexArrays(1, &quadVAO);
        glDeleteBuffers(1, &quadVBO);
        glDeleteBuffers(1, &triangleSSBO);

        boxMesh->cleanup();
        delete boxMesh;
        boxMaterial.cleanup();

        sphereMesh->cleanup();
        delete sphereMesh;
        sphereMaterial.cleanup();

        for (auto* box : boxes) {
            delete box;
        }
        
        groundMesh->cleanup();
        delete groundMesh;
        groundMaterial.cleanup();
    } // End of scope

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

// Process keyboard input
void processInput(GLFWwindow *window, RigidSolver& solver, std::vector<Object*>& dynamicObjects, Mesh* sphereMesh, Material* sphereMat)
{
    static bool leftMousePressed = false;
    static bool rightMousePressed = false;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    
    // Visualization Toggles (G: Wireframe, F: Fill)
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
        wireframeMode = true;
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        wireframeMode = false;
    
    // Raytracing Toggle (R: Raytracing, T: Standard)
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        raytracingMode = true;
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
        raytracingMode = false;

    // LEFT MOUSE - Bolt Sphere
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        if (!leftMousePressed) {
            // 1. Get Mouse Position and convert to NDC
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            
            float x = (2.0f * xpos) / SCR_WIDTH - 1.0f;
            float y = 1.0f - (2.0f * ypos) / SCR_HEIGHT;

            // 2. Calculate Ray Direction in world space
            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
            glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 invProj = glm::inverse(projection);
            glm::mat4 invView = glm::inverse(view);

            glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);
            glm::vec4 rayEye = invProj * rayClip;
            rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
            glm::vec3 rayWorld = glm::normalize(glm::vec3(invView * rayEye));

            // 3. Create Sphere
            Object* bolt = new Object(sphereMesh, sphereMat);
            bolt->setPosition(cameraPos);
            bolt->setScale(glm::vec3(0.5f)); // Visual radius is 0.5
            bolt->setAsSphere(0.5f, 10.0f); // Collision radius 0.5 to match visual scale
            bolt->velocity = rayWorld * 20.0f; // Fast bolt!
            bolt->fixedObject = false;
            bolt->restitution = 0.8f;

            dynamicObjects.push_back(bolt);
            solver.addObject(bolt);

            leftMousePressed = true;
        }
    } else {
        leftMousePressed = false;
    }

    // RIGHT MOUSE - Big Drop Sphere
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        if (!rightMousePressed) {
            // Drop high above the origin
            glm::vec3 spawnPos = glm::vec3(0.0f, 15.0f, 0.0f);

            // Create Large Sphere
            Object* bigSphere = new Object(sphereMesh, sphereMat);
            bigSphere->setPosition(spawnPos);
            bigSphere->setScale(glm::vec3(3.0f)); // Big visual radius
            bigSphere->setAsSphere(3.0f, 15.0f); // Collision radius 3.0 to match visual scale
            bigSphere->velocity = glm::vec3(0.0f); // No start velocity
            bigSphere->fixedObject = false;
            bigSphere->restitution = 0.3f; // Less bouncy since it's heavy

            dynamicObjects.push_back(bigSphere);
            solver.addObject(bigSphere);

            rightMousePressed = true;
        }
    } else {
        rightMousePressed = false;
    }
}

// Handle window resize events
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	// Adjust viewport to match new window dimensions
	glViewport(0, 0, width, height);
}

// Handle mouse scroll for zooming
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    cameraDistance -= static_cast<float>(yoffset) * zoomSpeed;
    cameraDistance = glm::clamp(cameraDistance, 1.0f, 30.0f);  // Limit zoom range
}

// Initialize GLFW and create window
GLFWwindow *initializeGLFW()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Final Project", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, scroll_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return nullptr;
    }

    return window;
}
