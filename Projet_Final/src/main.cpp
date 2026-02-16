#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "shader.h"
#include "object.h"
#include "icosahedron.h"
#include "rigidsolver.h"

#include <iostream>
#include <cmath>
#include <vector>

// Function declarations
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void updateCameraPosition();
GLFWwindow *initializeGLFW();

// Window settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;

// Camera settings - Spherical coordinates
float cameraDistance = 10.0f;
float cameraTheta = 0.0f;      // Horizontal angle (around Y axis)
float cameraPhi = M_PI / 2.0f; // Vertical angle (from Y axis)

glm::vec3 cameraPos;
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

// Camera movement
float rotationSpeed = 0.05f;
float zoomSpeed = 0.5f;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float timeAccumulator = 0.0f;

bool wireframeMode = false;

int main()
{
	GLFWwindow *window = initializeGLFW();
	if (!window)
		return -1;

	glEnable(GL_DEPTH_TEST);

	Shader shaderProgram("src/vertex_shader.glsl", "src/fragment_shader.glsl");
	Shader shadowShader("src/shadow_vertex.glsl", "src/shadow_fragment.glsl");

	// Create shadow map framebuffer
	const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);

	// Create depth texture
	unsigned int depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	// Attach depth texture to framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Create shared Box assets
    Mesh* boxMesh = new Mesh({}, {});
    boxMesh->addCube(1.0f);
    boxMesh->subdivideLinear(); 
    boxMesh->subdivideLinear();
    Material boxMaterial;

    std::vector<Object*> boxes;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            for (int z = 0; z < 4; ++z) {
                Object* box = new Object(boxMesh, &boxMaterial);
                // Center the 4x4 stack. Each box is 1x1x1.
                // Floor is at -2.0. So bottom boxes (y=0) should be at -1.5.
                box->setPosition(glm::vec3(x - 1.5f, y - 1.5f, z - 1.5f));
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

    updateCameraPosition();

	// Render loop
	while (!glfwWindowShouldClose(window))
	{
        // Calculate delta time
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Update physics using sub-stepping for better stability
        float fixedTimeStep = 0.005f; // 200 Hz simulation
        timeAccumulator += deltaTime;
        while (timeAccumulator >= fixedTimeStep) {
            solver.step(fixedTimeStep);
            timeAccumulator -= fixedTimeStep;
        }

        // Light properties
        glm::vec3 lightPos(3.0f, 5.0f, 2.0f);
        glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

        // Light space matrix for shadow mapping
        float near_plane = 1.0f, far_plane = 20.0f;
        glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        // ======= PASS 1: Render depth map from light's perspective =======
        shadowShader.use();
        shadowShader.set("lightSpaceMatrix", lightSpaceMatrix);
        
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        
        // Enable face culling to prevent self-shadowing (Peter Panning solution)
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);  // Cull front faces during shadow map rendering
        
        // Render scene from light's perspective
        ground.draw(shadowShader);
        for (auto* box : boxes) {
            box->draw(shadowShader);
        }
        
        glCullFace(GL_BACK);  // Reset to default
        glDisable(GL_CULL_FACE);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ======= PASS 2: Render scene normally with shadows =======
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (wireframeMode)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        shaderProgram.use();
        
        // Bind shadow map
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        shaderProgram.set("shadowMap", 0);
        
        // Light properties
        shaderProgram.set("lightPos", lightPos);
        shaderProgram.set("lightColor", lightColor);
        shaderProgram.set("viewPos", cameraPos);
        shaderProgram.set("lightSpaceMatrix", lightSpaceMatrix);
        
        // Transformation matrices
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        
        shaderProgram.set("view", view);
        shaderProgram.set("projection", projection);

        // Draw ground
        shaderProgram.set("objectColor", 0.3f, 0.5f, 0.3f);  // Green ground
        ground.draw(shaderProgram);

        // Draw all boxes
        shaderProgram.set("objectColor", 0.4f, 0.4f, 0.8f); 
        for (auto* box : boxes) {
            box->draw(shaderProgram);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
	}

	// Cleanup resources and terminate GLFW
    boxMesh->cleanup();
    delete boxMesh;
    boxMaterial.cleanup();
    for (auto* box : boxes) {
        delete box;
    }
    
	groundMesh->cleanup();
	delete groundMesh;
	groundMaterial.cleanup();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

// Process keyboard input
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    
    // Visualization Toggles (W: Wireframe, F: Fill)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        wireframeMode = true;
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        wireframeMode = false;

    // Camera Controls (Arrow Keys)
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        cameraPhi -= rotationSpeed; 
        updateCameraPosition();
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        cameraPhi += rotationSpeed;
        updateCameraPosition();
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
    {
        cameraTheta -= rotationSpeed; 
        updateCameraPosition();
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    {
        cameraTheta += rotationSpeed; 
        updateCameraPosition();
    }
}

// Handle window resize events
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	// Adjust viewport to match new window dimensions
	glViewport(0, 0, width, height);
}

// Update camera position from spherical coordinates
void updateCameraPosition()
{
    // Clamp phi to avoid gimbal lock
    cameraPhi = glm::clamp(cameraPhi, 0.1f, (float)M_PI - 0.1f);
    
    // Convert spherical to cartesian coordinates
    cameraPos.x = cameraDistance * sin(cameraPhi) * cos(cameraTheta);
    cameraPos.y = cameraDistance * cos(cameraPhi);
    cameraPos.z = cameraDistance * sin(cameraPhi) * sin(cameraTheta);
}

// Handle mouse scroll for zooming
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    cameraDistance -= static_cast<float>(yoffset) * zoomSpeed;
    cameraDistance = glm::clamp(cameraDistance, 1.0f, 20.0f);  // Limit zoom range
    updateCameraPosition();
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
