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
    glm::vec3 skyTop = glm::vec3(0.5f, 0.7f, 1.0f);
    glm::vec3 skyBottom = glm::vec3(1.0f, 1.0f, 1.0f);

    Scene();
    virtual ~Scene(); // Virtual destructor is crucial for base classes

    virtual void step(float dt);
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
    Mesh* boxMesh = nullptr;
    std::vector<Material*> boxMaterials;
    Mesh* groundMesh = nullptr;
    Material* groundMaterial = nullptr;
};


class RayTracingScene : public Scene {
public:
    RayTracingScene();
    virtual ~RayTracingScene();
    virtual void processInput(GLFWwindow* window, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection) override;
private:
    Material *silverMat = nullptr;
    Material *glassMat = nullptr;
    Material *matteMat = nullptr;
    Material *goldMat = nullptr;
    Material *metalMat = nullptr;
    Material *sageMat = nullptr;
    Material *lightMat = nullptr;
    Mesh *lightMesh = nullptr;
};

class MirrorScene : public Scene {
public:
    MirrorScene();
    virtual ~MirrorScene();
    virtual void processInput(GLFWwindow* window, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection) override;
private:
    Mesh *planeMesh = nullptr;
    Mesh *lightMesh = nullptr;
    Material *mirrorMat = nullptr;
    Material *leftMirrorMat = nullptr;
    Material *rightMirrorMat = nullptr;
    Material *lightMat = nullptr;
    Mesh *cubeMesh = nullptr;
    Material *cubeMat = nullptr;
};

class DarkScene : public Scene {
public: 
    DarkScene();
    virtual ~DarkScene();
    virtual void processInput(GLFWwindow* window, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection) override;
private:
    Mesh *sphereMesh = nullptr;
    Mesh *planeMesh = nullptr;
    Mesh *cubeMesh = nullptr;
    Material *sphereMat = nullptr;
    Material *mirrorMat = nullptr;
    Material *redLightMat = nullptr;
    Material *blueLightMat = nullptr;
    Material *greenLightMat = nullptr;
    Material *floorMat = nullptr;
    Material *occluderMat = nullptr;
};

class SeaScene : public Scene {
public:
    SeaScene();
    virtual ~SeaScene();
    virtual void processInput(GLFWwindow* window, const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& projection) override;
    virtual void step(float dt) override;

private:
    Mesh *seaMesh = nullptr;
    Material *seaMat = nullptr;
    Object *seaObject = nullptr;
    
    // Deep layer to simulate thickness
    Object *seaBottomObject = nullptr;
    Material *seaBottomMat = nullptr;

    Object *sun = nullptr;
    Mesh *sunMesh = nullptr;
    Material *sunMat = nullptr;
    float time = 0.0f;

    int gridRes = 15;
    float seaSize = 100.0f;
};

#endif // SCENE_H

