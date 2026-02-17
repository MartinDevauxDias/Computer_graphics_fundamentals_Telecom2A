#include "scene.h"
#include "icosahedron.h"
#include <GLFW/glfw3.h>
#include <iostream>

// --- Base Scene Implementation ---

Scene::Scene() : solver(glm::vec3(0.0f, -9.81f, 0.0f), -2.0f) {}

// Virtual destructor implementation
Scene::~Scene() {
    std::cout << "Base Scene destructor called." << std::endl;
    for (auto* obj : objects) {
        // This assumes derived scenes manage the memory of their specific objects/assets
        delete obj;
    }
    objects.clear();
}

void Scene::step(float dt) {
    solver.step(dt);
}

void Scene::addObject(Object* obj) {
    objects.push_back(obj);
    solver.addObject(obj);
}

// Base implementation does nothing.
void Scene::processInput(GLFWwindow* window, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection) {}


// --- PhysicsStackScene Implementation ---

PhysicsStackScene::PhysicsStackScene() {
    // Create assets needed for this specific scene
    SphereMesh = Icosahedron::createIcosphere(1.0f, 2);
    SphereMaterial = new Material();
    SphereMaterial->diffuse = glm::vec3(1.0f, 0.2f, 0.2f);

    // Create shared assets for the boxes and ground
    Mesh* boxMesh = new Mesh({}, {});
    boxMesh->addCube(1.0f);
    boxMesh->subdivideLinear();
    boxMesh->subdivideLinear();
    Material* boxMaterial = new Material();

    Mesh* groundMesh = new Mesh({}, {});
    groundMesh->addPlan(15.0f);
    Material* groundMaterial = new Material();

    // Create the stack of boxes
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            for (int z = 0; z < 4; ++z) {
                Object* box = new Object(boxMesh, boxMaterial);
                box->setPosition(glm::vec3(x - 1.5f, y - 1.5f, z - 1.5f));
                box->setAsBox(1.0f, 1.0f, 1.0f, 5.0f);
                this->addObject(box);
            }
        }
    }

    // Create ground
    Object* ground = new Object(groundMesh, groundMaterial);
    ground->setPosition(glm::vec3(0.0f, -2.0f, 0.0f));
    ground->fixedObject = true;
    this->addObject(ground);
}

PhysicsStackScene::~PhysicsStackScene() {
    std::cout << "PhysicsStackScene destructor called." << std::endl;
    // Clean up the assets owned ONLY by this scene
    if (SphereMesh) {
        SphereMesh->cleanup();
        delete SphereMesh;
    }
    if (SphereMaterial) {
        delete SphereMaterial;
    }
    // The base class destructor will handle cleaning the `objects` vector.
}

void PhysicsStackScene::processInput(GLFWwindow* window, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection) {
    static bool leftMousePressed = false;
    static bool rightMousePressed = false;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        if (!leftMousePressed) {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            float x = (2.0f * xpos) / width - 1.0f;
            float y = 1.0f - (2.0f * ypos) / height;

            glm::vec4 rayClip = glm::vec4(x, y, -1.0, 1.0);
            glm::vec4 rayEye = glm::inverse(projection) * rayClip;
            rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0, 0.0);
            glm::vec3 rayWorld = glm::normalize(glm::vec3(glm::inverse(view) * rayEye));

            Object* bolt = new Object(this->SphereMesh, this->SphereMaterial);
            bolt->setPosition(cameraPos);
            bolt->setScale(glm::vec3(0.5f));
            bolt->setAsSphere(0.5f, 10.0f);
            bolt->velocity = rayWorld * 20.0f;
            this->addObject(bolt);

            leftMousePressed = true;
        }
    } else {
        leftMousePressed = false;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        if (!rightMousePressed) {
            // Drop high above the origin
            glm::vec3 spawnPos = glm::vec3(0.0f, 15.0f, 0.0f);

            // Create Large Sphere
            Object* bigSphere = new Object(SphereMesh, SphereMaterial);
            bigSphere->setPosition(spawnPos);
            bigSphere->setScale(glm::vec3(3.0f)); // Big visual radius
            bigSphere->setAsSphere(3.0f, 15.0f); // Collision radius 3.0 to match visual scale
            bigSphere->velocity = glm::vec3(0.0f); // No start velocity
            bigSphere->restitution = 0.3f; // Less bouncy since it's heavy
            this->addObject(bigSphere);

            rightMousePressed = true;
        }
    } else {
        rightMousePressed = false;
    }
}

void Scene::movementInput(GLFWwindow *window, bool wireframeMode, bool raytracingMode)
{
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
}