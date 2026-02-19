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


void Scene::processInput(GLFWwindow *window, const glm::vec3 &cameraPos, const glm::mat4 &view, const glm::mat4 &projection) {}

// --- PhysicsStackScene Implementation ---

PhysicsStackScene::PhysicsStackScene()
{
    // Create assets needed for this specific scene
    SphereMesh = Icosahedron::createIcosphere(1.0f, 2);
    SphereMaterial = new Material();
    SphereMaterial->diffuse = glm::vec3(0.6f, 0.1f, 0.1f);
    SphereMaterial->reflectivity = 0.9f;
    SphereMaterial->roughness = 0.0f;

    // Create shared assets for the boxes and ground
    boxMesh = new Mesh({}, {});
    boxMesh->addCube(1.0f);
    for (int i = 0; i < 1; ++i)
        boxMesh->subdivideLinear();
    
    std::vector<glm::vec3> colors = {
        glm::vec3(0.937f, 0.325f, 0.314f), // Soft Vivid Red
        glm::vec3(1.0f, 0.655f, 0.150f),   // Soft Vivid Orange
        glm::vec3(0.937f, 0.933f, 0.345f), // Soft Vivid Yellow
        glm::vec3(0.400f, 0.733f, 0.416f), // Soft Vivid Green
        glm::vec3(0.259f, 0.647f, 0.960f), // Soft Vivid Blue
        glm::vec3(0.670f, 0.278f, 0.737f)  // Soft Vivid Purple
    };

    for (const auto& color : colors) {
        Material* m = new Material();
        m->diffuse = color;
        boxMaterials.push_back(m);
    }

    groundMesh = new Mesh({}, {});
    groundMesh->addPlan(15.0f);
    groundMaterial = new Material();
    groundMaterial->diffuse = glm::vec3(0.5f, 0.5f, 0.5f);
    groundMaterial->reflectivity = 0.4f; // Semi-reflective floor
    groundMaterial->roughness = 0.0f;

    // Create the stack of boxes
    float groundLevel = -2.0f;
    int colorIdx = 0;
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            for (int z = 0; z < 4; ++z)
            {
                Object *box = new Object(boxMesh, boxMaterials[colorIdx % boxMaterials.size()]);
                box->setPosition(glm::vec3(
                    (x - 1.5f) * 1.0f,
                    groundLevel + 0.5f + y * 1.0f,
                    (z - 1.5f) * 1.0f));
                box->setAsBox(1.0f, 1.0f, 1.0f, 1.0f);
                box->restitution = 0.0f;
                this->addObject(box);
                colorIdx++;
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
    if (SphereMesh)
    {
        SphereMesh->cleanup();
        delete SphereMesh;
    }
    if (SphereMaterial)
    {
        delete SphereMaterial;
    }
    if (boxMesh)
    {
        boxMesh->cleanup();
        delete boxMesh;
    }
    for (auto m : boxMaterials)
    {
        delete m;
    }
    if (groundMesh)
    {
        groundMesh->cleanup();
        delete groundMesh;
    }
    if (groundMaterial)
    {
        delete groundMaterial;
    }
}

// --- RayTracingScene Implementation ---

RayTracingScene::RayTracingScene()
{
    // Create shared assets
    Mesh *sphere = Icosahedron::createIcosphere(1.0f, 3);
    Mesh *plane = new Mesh({}, {});
    plane->addPlan(50.0f);

    Material *groundMat = new Material();
    groundMat->diffuse = glm::vec3(0.1f, 0.1f, 0.12f);
    groundMat->reflectivity = 0.4f; 
    groundMat->roughness = 0.3f; 
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
    wallMirrorMat->diffuse = glm::vec3(0.85f, 0.85f, 1.0f); 
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
    float floorY = -2.0f + r;

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

    // --- Physical Sun ---
    this->skyTop = glm::vec3(0.15f, 0.21f, 0.3f);    
    this->skyBottom = glm::vec3(0.4f, 0.4f, 0.45f);  

    lightMesh = Icosahedron::createIcosphere(1.0f, 2);
    lightMat = new Material();
    lightMat->emissive = glm::vec3(1.0f, 0.9f, 0.7f);
    lightMat->emissiveStrength = 50.0f;

    Object *sun = new Object(lightMesh, lightMat);
    sun->setPosition(glm::vec3(-50.0f, 30.0f, -10.0f));
    sun->setScale(glm::vec3(5.0f));
    sun->isSphere = true;
    sun->fixedObject = true;
    this->addObject(sun);
}

RayTracingScene::~RayTracingScene() {
    if (lightMesh) {
        lightMesh->cleanup();
        delete lightMesh;
    }
    if (lightMat) delete lightMat;
    if (silverMat) delete silverMat;
    if (glassMat) delete glassMat;
    if (matteMat) delete matteMat;
    if (goldMat) delete goldMat;
    if (metalMat) delete metalMat;
    if (sageMat) delete sageMat;
}

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
            bolt->setAsSphere(0.5f, 2.0f);      
            bolt->velocity = cameraFront * 40.0f;
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
            glm::vec3 spawnPos = glm::vec3(0.0f, 15.0f, 0.0f);

            // Create Large Sphere
            Object *bigSphere = new Object(SphereMesh, SphereMaterial);
            bigSphere->setPosition(spawnPos);
            bigSphere->setScale(glm::vec3(3.0f));  
            bigSphere->setAsSphere(3.0f, 10.0f);  
            bigSphere->velocity = glm::vec3(0.0f); 
            bigSphere->restitution = 0.1f;    
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
    mirrorMat->diffuse = glm::vec3(0.90f); 
    mirrorMat->reflectivity = 1.0f;
    mirrorMat->roughness = 0.0f;

    leftMirrorMat = new Material();
    leftMirrorMat->diffuse = glm::vec3(0.3f, 0.3f, 0.8f);
    leftMirrorMat->reflectivity = 0.9f;                   // 90% Sharp reflection, 10% Diffuse
    leftMirrorMat->roughness = 0.0f;

    rightMirrorMat = new Material();
    rightMirrorMat->diffuse = glm::vec3(0.3f, 0.8f, 0.3f); 
    rightMirrorMat->reflectivity = 0.9f;                    // 90% Sharp reflection, 10% Diffuse
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
    leftWall->setRotation(glm::vec3(0, 0, -90)); 
    leftWall->fixedObject = true;
    this->addObject(leftWall);

    // Right Wall (Position +X, needs Normal -X to face center)
    Object *rightWall = new Object(planeMesh, rightMirrorMat);
    rightWall->setPosition(glm::vec3(halfSize, elevation, 0));
    rightWall->setRotation(glm::vec3(0, 0, 90)); 
    rightWall->fixedObject = true;
    this->addObject(rightWall);

    // --- Ceiling Light ---
    lightMesh = new Mesh({}, {});
    lightMesh->addPlan(3.0f); // A 4x4 light panel

    lightMat = new Material();
    lightMat->emissive = glm::vec3(1.0f, 0.95f, 0.85f); // Warm white light
    lightMat->emissiveStrength = 20.0f;

    Object *light = new Object(lightMesh, lightMat);
    light->setPosition(glm::vec3(0, halfSize - 0.5f + elevation, 0)); 
    light->setRotation(glm::vec3(180, 0, 0));                       
    light->fixedObject = true;
    this->objects.push_back(light);

    // --- Physical reference ---
    cubeMesh = new Mesh({}, {});
    cubeMesh->addCube(1.0f);

    cubeMat = new Material();
    cubeMat->diffuse = glm::vec3(0.7f, 0.1f, 0.1f); 
    cubeMat->reflectivity = 0.2f;                  
    cubeMat->roughness = 0.0f;
    cubeMat->transparency = 0.40f;
    cubeMat->ior = 1.50f;

    Object *centerCube = new Object(cubeMesh, cubeMat);
    centerCube->setPosition(glm::vec3(0, -halfSize + elevation + 5.0f, -5.0f));
    centerCube->setScale(glm::vec3(5.0f));
    centerCube->setRotation(glm::vec3(0.0f, 45.0f, 45.0f)); 
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

DarkScene::DarkScene()
{
    this->skyTop = glm::vec3(0.01f, 0.01f, 0.01f);   
    this->skyBottom = glm::vec3(0.05f, 0.05f, 0.05f);

    sphereMesh = Icosahedron::createIcosphere(1.0f, 3);
    planeMesh = new Mesh({}, {});
    planeMesh->addPlan(50.0f);
    cubeMesh = new Mesh({}, {});
    cubeMesh->addCube(1.0f);

    // 1. Center Sphere (Grey)
    sphereMat = new Material();
    sphereMat->diffuse = glm::vec3(0.5f, 0.5f, 0.5f);
    sphereMat->roughness = 0.5f;
    sphereMat->reflectivity = 0.2f; // Slightly reflective to show light interaction
    Object *centerSphere = new Object(sphereMesh, sphereMat);
    centerSphere->setPosition(glm::vec3(0, 0, 0));
    centerSphere->setScale(glm::vec3(1.5f));
    centerSphere->isSphere = true;
    centerSphere->fixedObject = true;
    this->addObject(centerSphere);

    // 2. Floor
    floorMat = new Material();
    floorMat->diffuse = glm::vec3(0.1f, 0.1f, 0.1f);
    floorMat->roughness = 0.8f;
    Object *floor = new Object(planeMesh, floorMat);
    floor->setPosition(glm::vec3(0, -2.0f, 0));
    floor->fixedObject = true;
    this->addObject(floor);

    // --- Red Light Path (Mirror Bounce) ---
    // Mirror for the red light (on the left)
    mirrorMat = new Material();
    mirrorMat->reflectivity = 1.0f;
    mirrorMat->roughness = 0.0f;
    Object *redMirror = new Object(planeMesh, mirrorMat);
    redMirror->setPosition(glm::vec3(-5, 0, 0));
    redMirror->setRotation(glm::vec3(0, 0, -45)); // Reflects upwards light towards the origin
    redMirror->setScale(glm::vec3(0.1f)); 
    redMirror->fixedObject = true;
    this->addObject(redMirror);

    // Red Light (above the mirror)
    redLightMat = new Material();
    redLightMat->emissive = glm::vec3(1.0f, 0.05f, 0.05f);
    redLightMat->emissiveStrength = 60.0f;
    Object *redLight = new Object(sphereMesh, redLightMat);
    redLight->setPosition(glm::vec3(-5, 5, 0));
    redLight->setScale(glm::vec3(0.4f));
    redLight->isSphere = true;
    redLight->fixedObject = true;
    this->addObject(redLight);

    // Occluder (to hide red light from sphere)
    occluderMat = new Material();
    occluderMat->diffuse = glm::vec3(0.05f, 0.05f, 0.05f);
    Object *occluder = new Object(cubeMesh, occluderMat);
    occluder->setPosition(glm::vec3(-2.5f, 2.5f, 0)); // Between red light and sphere
    occluder->setScale(glm::vec3(1.0f, 3.0f, 2.0f));
    occluder->setRotation(glm::vec3(0, 0, -45)); // Same angle as mirror to block direct line
    occluder->fixedObject = true;
    this->addObject(occluder);

    // --- Other Lights (Closer) ---
    // Blue Light (Front Right)
    blueLightMat = new Material();
    blueLightMat->emissive = glm::vec3(0.05f, 0.05f, 1.0f);
    blueLightMat->emissiveStrength = 30.0f;
    Object *blueLight = new Object(sphereMesh, blueLightMat);
    blueLight->setPosition(glm::vec3(-3, -1, 3));
    blueLight->setScale(glm::vec3(0.3f));
    blueLight->isSphere = true;
    blueLight->fixedObject = true;
    this->addObject(blueLight);

    // Green Light (Right Side - opposite to the red mirror)
    greenLightMat = new Material();
    greenLightMat->emissive = glm::vec3(0.05f, 1.0f, 0.05f);
    greenLightMat->emissiveStrength = 30.0f;
    Object *greenLight = new Object(sphereMesh, greenLightMat);
    greenLight->setPosition(glm::vec3(5, 1, -2));
    greenLight->setScale(glm::vec3(0.3f));
    greenLight->isSphere = true;
    greenLight->fixedObject = true;
    this->addObject(greenLight);
}

DarkScene::~DarkScene()
{
    if (sphereMesh) { sphereMesh->cleanup(); delete sphereMesh; }
    if (planeMesh) { planeMesh->cleanup(); delete planeMesh; }
    if (cubeMesh) { cubeMesh->cleanup(); delete cubeMesh; }
    if (sphereMat) delete sphereMat;
    if (mirrorMat) delete mirrorMat;
    if (redLightMat) delete redLightMat;
    if (blueLightMat) delete blueLightMat;
    if (greenLightMat) delete greenLightMat;
    if (floorMat) delete floorMat;
    if (occluderMat) delete occluderMat;
}

void DarkScene::processInput(GLFWwindow *window, const glm::vec3 &cameraPos, const glm::mat4 &view, const glm::mat4 &projection) {}

// --- SeaScene Implementation ---

SeaScene::SeaScene()
{
    this->skyTop = glm::vec3(0.1f, 0.2f, 0.4f);  
    this->skyBottom = glm::vec3(0.8f, 0.4f, 0.1f); 

    // Create a high-resolution grid for the sea
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float stepSize = seaSize / (gridRes - 1);
    for (int z = 0; z < gridRes; ++z)
    {
        for (int x = 0; x < gridRes; ++x)
        {
            float posX = x * stepSize - seaSize / 2.0f;
            float posZ = z * stepSize - seaSize / 2.0f;
            
            // Match the height formula at time = 0 to avoid jumping
            float h = 0.0f;
            h += 0.4f * (1.0f - std::abs(std::sin(0.15f * posX)));
            h += 0.25f * (1.0f - std::abs(std::sin(0.12f * posZ + 1.0f)));
            h += 0.15f * (1.0f - std::abs(std::sin(0.08f * (posX + posZ))));

            Vertex v;
            v.position = glm::vec3(posX, h - 3.0f, posZ);
            v.normal = glm::vec3(0, 1, 0);
            v.texCoords = glm::vec2((float)x / (gridRes-1), (float)z / (gridRes-1));
            vertices.push_back(v);
        }
    }

    for (int z = 0; z < gridRes - 1; ++z)
    {
        for (int x = 0; x < gridRes - 1; ++x)
        {
            int i0 = x + z * gridRes;
            int i1 = (x + 1) + z * gridRes;
            int i2 = x + (z + 1) * gridRes;
            int i3 = (x + 1) + (z + 1) * gridRes;

            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
        }
    }

    seaMesh = new Mesh(vertices, indices);
    seaMat = new Material();
    // A mix of deep blue and surface scattering
    seaMat->diffuse = glm::vec3(0.1f, 0.4f, 0.6f); 
    seaMat->reflectivity = 0.3f;  // Surface glints
    seaMat->roughness = 0.0f; 
    seaMat->transparency = 0.9f;  // Multi-path: 50% Dielectric, others use diffuse/reflect
    seaMat->ior = 1.33f;         // Water IOR

    seaObject = new Object(seaMesh, seaMat);
    seaObject->fixedObject = false; // Keep non-fixed so renderer resets accumulation every frame
    this->objects.push_back(seaObject); // DON'T use addObject to bypass physics solver

    // --- Deep Water Layer ---
    seaBottomMat = new Material();
    seaBottomMat->diffuse = glm::vec3(0.05f, 0.6f, 0.8f); // Very dark deep blue
    seaBottomMat->reflectivity = 0.95f;                   // Mirror-like depth
    seaBottomMat->roughness = 0.0f;
    seaBottomMat->transparency = 0.1f;                    // Opaque floor

    seaBottomObject = new Object(seaMesh, seaBottomMat); // Reuse the same mesh
    seaBottomObject->fixedObject = false;
    this->objects.push_back(seaBottomObject);

    // --- Rising Sun ---
    sunMesh = Icosahedron::createIcosphere(1.0f, 2);
    sunMat = new Material();
    sunMat->emissive = glm::vec3(1.0f, 0.6f, 0.2f); // Deep orange/yellow
    sunMat->emissiveStrength = 100.0f; // Boosted for the further distance

    sun = new Object(sunMesh, sunMat);
    sun->setPosition(glm::vec3(0.0f, 5.0f, -120.0f)); 
    sun->setScale(glm::vec3(12.0f));
    sun->isSphere = true;
    sun->fixedObject = false; // Non-fixed to trigger renderer reset
    this->objects.push_back(sun); 
}

SeaScene::~SeaScene()
{
    if (seaMesh) { seaMesh->cleanup(); delete seaMesh; }
    if (seaMat) delete seaMat;
    if (seaBottomMat) delete seaBottomMat;
    if (sunMesh) { sunMesh->cleanup(); delete sunMesh; }
    if (sunMat) delete sunMat;
}

void SeaScene::step(float dt)
{
    time += dt;

    // Update sea vertices to create waves
    float t = time * 0.4f; // Slower time
    for (int i = 0; i < seaMesh->vertices.size(); ++i)
    {
        float x = seaMesh->vertices[i].position.x;
        float z = seaMesh->vertices[i].position.z;

        // Sharper "triangly" peaks using 1.0 - |sin|
        // Also reduced amplitude and slowed down the spatial frequencies
        float h = 0.0f;
        h += 0.4f * (1.0f - std::abs(std::sin(0.15f * x + 1.2f * t)));
        h += 0.25f * (1.0f - std::abs(std::sin(0.12f * z + 0.8f * t + 1.0f)));
        h += 0.15f * (1.0f - std::abs(std::sin(0.08f * (x + z) + 1.5f * t)));

        seaMesh->vertices[i].position.y = h - 3.0f; // Lowered baseline to stay below camera
    }
    
    // Recompute normals and update GPU buffers for the animated mesh
    seaMesh->recomputeNormals();
    seaMesh->updateBuffers();

    // Offset the bottom layer slightly below the surface to avoid Z-fighting 
    // and provide that "thickness" look
    seaBottomObject->setPosition(glm::vec3(0.0f, -0.2f, 0.0f)); 
    
    // Smoothly rise the sun (slower)
    glm::vec3 sunPos = sun->position;
    sunPos.y = 5.0f + 2.0f * std::sin(time * 0.05f); 
    sun->setPosition(sunPos);

    Scene::step(dt);
}

void SeaScene::processInput(GLFWwindow *window, const glm::vec3 &cameraPos, const glm::mat4 &view, const glm::mat4 &projection) {}
