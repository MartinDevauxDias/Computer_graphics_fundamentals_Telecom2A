#include "scene.h"
#include "window.h"
#include <GLFW/glfw3.h>
#include <iostream>

// --- Base Scene Implementation ---

Scene::Scene() : solver(glm::vec3(0.0f, -9.81f, 0.0f), -2.0f) {}

// Virtual destructor implementation
Scene::~Scene()
{
    std::cout << "Base Scene destructor called." << std::endl;
    for (auto *obj : objects)
    {
        // This assumes derived scenes manage the memory of their specific objects/assets
        delete obj;
    }
    objects.clear();
}

void Scene::step(float dt)
{
    solver.step(dt);
}

void Scene::addObject(Object *obj)
{
    objects.push_back(obj);
    solver.addObject(obj);
}

// Base implementation does nothing.
void Scene::processInput(GLFWwindow *window, const glm::vec3 &cameraPos, const glm::mat4 &view, const glm::mat4 &projection) {}

// --- PhysicsStackScene Implementation ---

PhysicsStackScene::PhysicsStackScene()
{
    // Create assets needed for this specific scene
    SphereMesh = Icosahedron::createIcosphere(1.0f, 2);
    SphereMaterial = new Material();
    SphereMaterial->diffuse = glm::vec3(0.9f, 0.9f, 0.9f);
    SphereMaterial->reflectivity = 0.9f;
    SphereMaterial->roughness = 0.05f;

    // Create shared assets for the boxes and ground
    Mesh *boxMesh = new Mesh({}, {});
    boxMesh->addCube(1.0f);
    for (int i = 0; i < 1; ++i)
        boxMesh->subdivideLinear();
    Material *boxMaterial = new Material();
    boxMaterial->diffuse = glm::vec3(0.4f, 0.4f, 0.8f);

    Mesh *groundMesh = new Mesh({}, {});
    groundMesh->addPlan(15.0f);
    Material *groundMaterial = new Material();
    groundMaterial->diffuse = glm::vec3(0.5f, 0.5f, 0.5f);
    groundMaterial->reflectivity = 0.4f; // Semi-reflective floor
    groundMaterial->roughness = 0.1f;

    // Create the stack of boxes
    float groundLevel = -2.0f;
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            for (int z = 0; z < 4; ++z)
            {
                Object *box = new Object(boxMesh, boxMaterial);
                box->setPosition(glm::vec3(
                    (x - 1.5f) * 1.0f,
                    groundLevel + 0.5f + y * 1.0f,
                    (z - 1.5f) * 1.0f));
                box->setAsBox(1.0f, 1.0f, 1.0f, 1.0f);
                box->restitution = 0.0f;
                this->addObject(box);
            }
        }
    }

    // Create ground
    Object *ground = new Object(groundMesh, groundMaterial);
    ground->setPosition(glm::vec3(0.0f, -2.0f, 0.0f));
    ground->fixedObject = true;
    ground->mass = 0.0f;
    ground->collisionRadius = 0.0f;
    this->objects.push_back(ground);
}

PhysicsStackScene::~PhysicsStackScene()
{
    std::cout << "PhysicsStackScene destructor called." << std::endl;
    // Clean up the assets owned ONLY by this scene
    if (SphereMesh)
    {
        SphereMesh->cleanup();
        delete SphereMesh;
    }
    if (SphereMaterial)
    {
        delete SphereMaterial;
    }
    // The base class destructor will handle cleaning the `objects` vector.
}

// --- RayTracingScene Implementation ---

RayTracingScene::RayTracingScene()
{
    // Create shared assets
    Mesh *sphere = Icosahedron::createIcosphere(1.0f, 3);
    Mesh *plane = new Mesh({}, {});
    plane->addPlan(50.0f);

    // Ground: Matte but noticeably reflective "Studio" floor
    Material *groundMat = new Material();
    groundMat->diffuse = glm::vec3(0.1f, 0.1f, 0.12f);
    groundMat->reflectivity = 0.4f; // More reflective
    groundMat->roughness = 0.3f;    // Slightly more polished but still matte
    Object *ground = new Object(plane, groundMat);
    ground->setPosition(glm::vec3(0, -2.0f, 0));
    ground->fixedObject = true;
    this->objects.push_back(ground);

    // Lateral Mirror Wall with Frame (on the right side)
    float mirrorWidth = 40.0f;
    float mirrorHeight = 15.0f;
    float mirrorCenterY = -2.0f + mirrorHeight / 2.0f;
    float mirrorCenterX = 20.0f;
    float mirrorCenterZ = -10.0f;

    Mesh *wallPlane = new Mesh({}, {});
    wallPlane->addPlan(20.0f); // 40x40 base
    Material *wallMirrorMat = new Material();
    wallMirrorMat->diffuse = glm::vec3(0.85f, 0.85f, 1.0f); // Soft tint
    wallMirrorMat->reflectivity = 1.0f;
    wallMirrorMat->roughness = 0.0f;
    Object *mirrorWall = new Object(wallPlane, wallMirrorMat);
    mirrorWall->setPosition(glm::vec3(mirrorCenterX, mirrorCenterY, mirrorCenterZ));
    mirrorWall->setRotation(glm::vec3(0, 0, 90));
    mirrorWall->setScale(glm::vec3(mirrorHeight / 40.0f, 1.0f, 1.0f)); // Scale Y-axis (local X because of rot)
    mirrorWall->fixedObject = true;
    this->addObject(mirrorWall);

    // Frame for the mirror (4 beams)
    Mesh *frameBox = new Mesh({}, {});
    frameBox->addCube(1.0f);
    frameBox->subdivideLinear();
    Material *frameMat = new Material();
    frameMat->diffuse = glm::vec3(0.1f, 0.1f, 0.1f);
    frameMat->reflectivity = 0.5f;
    frameMat->roughness = 0.2f;

    // Right/Left edges (vertical beams)
    for (float z_off : {-mirrorWidth / 2.0f, mirrorWidth / 2.0f})
    {
        Object *beam = new Object(frameBox, frameMat);
        beam->setPosition(glm::vec3(mirrorCenterX + 0.05f, mirrorCenterY, mirrorCenterZ + z_off));
        beam->setScale(glm::vec3(0.5f, mirrorHeight + 0.5f, 0.5f));
        beam->fixedObject = true;
        this->addObject(beam);
    }
    // Top/Bottom edges (horizontal beams)
    for (float y_off : {-mirrorHeight / 2.0f, mirrorHeight / 2.0f})
    {
        Object *beam = new Object(frameBox, frameMat);
        beam->setPosition(glm::vec3(mirrorCenterX + 0.05f, mirrorCenterY + y_off, mirrorCenterZ));
        beam->setScale(glm::vec3(0.5f, 0.5f, mirrorWidth + 0.5f));
        beam->fixedObject = true;
        this->addObject(beam);
    }

    // Common radius for most spheres to keep sizes similar
    float r = 2.0f;
    float floorY = -2.0f + r; // Position Y so they touch the ground

    // 1. Silver Mirror - Pushed further to the back-right
    silverMat = new Material();
    silverMat->diffuse = glm::vec3(0.95f, 0.95f, 1.0f);
    silverMat->reflectivity = 1.0f;
    silverMat->roughness = 0.0f;
    Object *s1 = new Object(sphere, silverMat);
    s1->setPosition(glm::vec3(12.0f, floorY, -22.0f));
    s1->setScale(glm::vec3(r));
    s1->fixedObject = true;
    s1->isSphere = true;
    this->addObject(s1);

    // 2. Crystal Glass - Pushed to the far-left
    glassMat = new Material();
    glassMat->transparency = 1.0f;
    glassMat->ior = 1.5f;
    glassMat->diffuse = glm::vec3(0.9f, 1.0f, 1.0f);
    Object *s2 = new Object(sphere, glassMat);
    s2->setPosition(glm::vec3(-11.0f, floorY, -13.0f));
    s2->setScale(glm::vec3(r));
    s2->fixedObject = true;
    s2->isSphere = true;
    this->addObject(s2);

    // 3. Matte Charcoal - Front-right with more space
    matteMat = new Material();
    matteMat->diffuse = glm::vec3(0.12f, 0.12f, 0.15f);
    matteMat->reflectivity = 0.0f;
    matteMat->roughness = 1.0f;
    Object *s3 = new Object(sphere, matteMat);
    s3->setPosition(glm::vec3(7.0f, floorY, -6.5f));
    s3->setScale(glm::vec3(r));
    s3->fixedObject = true;
    s3->isSphere = true;
    this->addObject(s3);

    // 4. Polished Gold - Deep center area
    goldMat = new Material();
    goldMat->diffuse = glm::vec3(1.0f, 0.8f, 0.2f);
    goldMat->reflectivity = 0.9f;
    goldMat->roughness = 0.05f;
    Object *s4 = new Object(sphere, goldMat);
    s4->setPosition(glm::vec3(2.0f, floorY, -18.0f));
    s4->setScale(glm::vec3(r));
    s4->fixedObject = true;
    s4->isSphere = true;
    this->addObject(s4);

    // 5. Burnished Copper - Front-left area
    metalMat = new Material();
    metalMat->diffuse = glm::vec3(0.8f, 0.4f, 0.25f);
    metalMat->reflectivity = 0.7f;
    metalMat->roughness = 0.2f;
    Object *s5 = new Object(sphere, metalMat);
    s5->setPosition(glm::vec3(-3.5f, floorY, -7.5f));
    s5->setScale(glm::vec3(r));
    s5->fixedObject = true;
    s5->isSphere = true;
    this->addObject(s5);

    // 6. Deep Sage - Far back-left corner
    sageMat = new Material();
    sageMat->diffuse = glm::vec3(0.35f, 0.45f, 0.35f);
    sageMat->reflectivity = 0.1f;
    sageMat->roughness = 0.8f;
    Object *s6 = new Object(sphere, sageMat);
    s6->setPosition(glm::vec3(-13.0f, floorY, -24.0f));
    s6->setScale(glm::vec3(r));
    s6->fixedObject = true;
    s6->isSphere = true;
    this->addObject(s6);
}

RayTracingScene::~RayTracingScene() {}

void RayTracingScene::processInput(GLFWwindow *window, const glm::vec3 &cameraPos, const glm::mat4 &view, const glm::mat4 &projection) {}

void PhysicsStackScene::processInput(GLFWwindow *window, const glm::vec3 &cameraPos, const glm::mat4 &view, const glm::mat4 &projection)
{
    // Get the window instance to access the cameraFront vector directly
    Window *win = static_cast<Window *>(glfwGetWindowUserPointer(window));
    glm::vec3 cameraFront = win ? win->cameraFront : glm::vec3(0, 0, -1);

    static bool leftMousePressed = false;
    static bool rightMousePressed = false;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        if (!leftMousePressed)
        {
            Object *bolt = new Object(this->SphereMesh, this->SphereMaterial);
            bolt->setPosition(cameraPos);
            bolt->setScale(glm::vec3(0.5f));
            bolt->setAsSphere(0.5f, 2.0f);        // Lower mass from 10.0 to 2.0
            bolt->velocity = cameraFront * 40.0f; // Directly use camera front
            this->addObject(bolt);

            leftMousePressed = true;
        }
    }
    else
    {
        leftMousePressed = false;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        if (!rightMousePressed)
        {
            // Drop high above the origin
            glm::vec3 spawnPos = glm::vec3(0.0f, 15.0f, 0.0f);

            // Create Large Sphere
            Object *bigSphere = new Object(SphereMesh, SphereMaterial);
            bigSphere->setPosition(spawnPos);
            bigSphere->setScale(glm::vec3(3.0f));  // Big visual radius
            bigSphere->setAsSphere(3.0f, 10.0f);   // Collision radius 3.0 to match visual scale
            bigSphere->velocity = glm::vec3(0.0f); // No start velocity
            bigSphere->restitution = 0.1f;         // Less bouncy since it's heavy
            this->addObject(bigSphere);

            rightMousePressed = true;
        }
    }
    else
    {
        rightMousePressed = false;
    }
}

MirrorScene::MirrorScene()
{
    float roomSize = 30.0f;
    float halfSize = roomSize / 2.0f;
    float elevation = 10.0f;

    planeMesh = new Mesh({}, {});
    planeMesh->addPlan(roomSize);

    mirrorMat = new Material();
    mirrorMat->diffuse = glm::vec3(0.95f); // Pure mirrors have no diffuse
    mirrorMat->reflectivity = 1.0f;
    mirrorMat->roughness = 0.0f;

    leftMirrorMat = new Material();
    leftMirrorMat->diffuse = glm::vec3(0.6f, 0.6f, 1.0f); // Teinte bleue
    leftMirrorMat->reflectivity = 1.0f;
    leftMirrorMat->roughness = 0.0f;

    rightMirrorMat = new Material();
    rightMirrorMat->diffuse = glm::vec3(0.6f, 1.0f, 0.6f); // Teinte verte
    rightMirrorMat->reflectivity = 1.0f;
    rightMirrorMat->roughness = 0.0f;

    // --- Create 6 walls using addObject for consistency ---

    // Floor (Normal +Y)
    Object *floor = new Object(planeMesh, mirrorMat);
    floor->setPosition(glm::vec3(0, -halfSize + elevation, 0));
    floor->fixedObject = true;
    this->addObject(floor);

    // Ceiling (Normal -Y)
    Object *ceiling = new Object(planeMesh, mirrorMat);
    ceiling->setPosition(glm::vec3(0, halfSize + elevation, 0));
    ceiling->setRotation(glm::vec3(180, 0, 0));
    ceiling->fixedObject = true;
    this->addObject(ceiling);

    // Back Wall (Normal +Z - facing camera)
    Object *backWall = new Object(planeMesh, mirrorMat);
    backWall->setPosition(glm::vec3(0, elevation, -halfSize));
    backWall->setRotation(glm::vec3(90, 0, 0));
    backWall->fixedObject = true;
    this->addObject(backWall);

    // Front Wall (Normal -Z - behind camera)
    Object *frontWall = new Object(planeMesh, mirrorMat);
    frontWall->setPosition(glm::vec3(0, elevation, halfSize));
    frontWall->setRotation(glm::vec3(-90, 0, 0));
    frontWall->fixedObject = true;
    this->addObject(frontWall);

    // Left Wall (Position -X, needs Normal +X to face center)
    Object *leftWall = new Object(planeMesh, leftMirrorMat);
    leftWall->setPosition(glm::vec3(-halfSize, elevation, 0));
    leftWall->setRotation(glm::vec3(0, 0, -90)); // Changed from 90 to -90
    leftWall->fixedObject = true;
    this->addObject(leftWall);

    // Right Wall (Position +X, needs Normal -X to face center)
    Object *rightWall = new Object(planeMesh, rightMirrorMat);
    rightWall->setPosition(glm::vec3(halfSize, elevation, 0));
    rightWall->setRotation(glm::vec3(0, 0, 90)); // Changed from -90 to 90
    rightWall->fixedObject = true;
    this->addObject(rightWall);

    // --- Ceiling Light ---
    lightMesh = new Mesh({}, {});
    lightMesh->addPlan(3.0f); // A 4x4 light panel

    lightMat = new Material();
    lightMat->emissive = glm::vec3(1.0f, 0.95f, 0.85f); // Warm white light
    lightMat->emissiveStrength = 10.0f;

    Object *light = new Object(lightMesh, lightMat);
    light->setPosition(glm::vec3(0, halfSize - 0.5f + elevation, 0)); // Slightly below the ceiling
    light->setRotation(glm::vec3(180, 0, 0));                         // Face down
    light->fixedObject = true;
    this->objects.push_back(light);

    // --- Physical reference ---
    cubeMesh = new Mesh({}, {});
    cubeMesh->addCube(1.0f);

    cubeMat = new Material();
    cubeMat->diffuse = glm::vec3(0.7f, 0.1f, 0.1f); // Rouge vif
    cubeMat->reflectivity = 0.2f;                   // Un peu de reflet de surface
    cubeMat->roughness = 0.0f;
    cubeMat->transparency = 0.40f;
    cubeMat->ior = 1.50f;

    Object *centerCube = new Object(cubeMesh, cubeMat);
    centerCube->setPosition(glm::vec3(0, -halfSize + elevation + 5.0f, -5.0f));
    centerCube->setScale(glm::vec3(5.0f));
    centerCube->setRotation(glm::vec3(0.0f, 45.0f, 45.0f)); // Penché sur deux axes
    centerCube->setAsBox(5.0f, 5.0f, 5.0f, 1.0f);
    centerCube->fixedObject = true;
    this->addObject(centerCube);
}

MirrorScene::~MirrorScene()
{
    if (!objects.empty())
    {
        delete planeMesh;
        delete mirrorMat;
        delete leftMirrorMat;
        delete rightMirrorMat;
        delete lightMesh;
        delete lightMat;
        delete cubeMesh;
        delete cubeMat;
    }
}

// This scene is not interactive, so processInput is empty.
void MirrorScene::processInput(GLFWwindow *window, const glm::vec3 &cameraPos, const glm::mat4 &view, const glm::mat4 &projection) {}