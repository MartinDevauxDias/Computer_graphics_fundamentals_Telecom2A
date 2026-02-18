#ifndef SCENE_H
#define SCENE_H

#include "object.h"
#include "rigidsolver.h"
#include <vector>
#include <glm/glm.hpp>

// Forward declare to avoid including glfw.h here
struct GLFWwindow;

// --- Base Scene Class ---
// Contains common data and functionality for all scenes.
class Scene {
public:
    std::vector<Object*> objects;
    RigidSolver solver;

    Scene();
    virtual ~Scene(); // Virtual destructor is crucial for base classes

    void step(float dt);
    void addObject(Object* obj);

    // Input handling is now virtual, to be overridden by derived scenes
    virtual void processInput(GLFWwindow* window, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection);
};


// --- Derived Scene for the Physics Demo ---
// Inherits from Scene and adds its own specific logic and data.
class PhysicsStackScene : public Scene {
public:
    PhysicsStackScene();
    virtual ~PhysicsStackScene();

    // Override the base processInput with our sphere-shooting logic
    virtual void processInput(GLFWwindow* window, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection) override;

private:
    // These assets belong ONLY to this scene
    Mesh* SphereMesh = nullptr;
    Material* SphereMaterial = nullptr;
};


class RayTracingScene : public Scene {
public:
    RayTracingScene();
    virtual ~RayTracingScene();
    virtual void processInput(GLFWwindow* window, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection) override;
};
#endif // SCENE_H
