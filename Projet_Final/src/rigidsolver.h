#ifndef RIGIDSOLVER_H
#define RIGIDSOLVER_H

#include <vector>
#include <glm/glm.hpp>
#include "object.h"

struct ContactConstraint {
    Object *objA, *objB;
    glm::vec3 contactPoint;
    glm::vec3 normal;
    float penetration;
    
    // PGS Cached values
    glm::vec3 rA, rB;
    float massNormal;     // 1 / (J M^-1 J^T)
    float bias;           // Baumgarte + Restitution
    float impulseSum;     // To clamp non-negative
    
    // Friction
    glm::vec3 tangent1, tangent2;
    float massTangent1, massTangent2;
    float impulseTangent1, impulseTangent2;
};

class RigidSolver {
public:
    // Simulation parameters
    glm::vec3 gravity;
    float floorY;

    RigidSolver(glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f), float floorY = -1.0f);

    // Add an object to the simulation
    void addObject(Object* object);

    // Step the simulation forward
    void step(float deltaTime);

    // Reset all objects to their initial state (optional)
    void reset();

private:
    std::vector<Object*> objects;
    std::vector<ContactConstraint> constraints;

    void detectCollisions();
    void solve(float dt);
    
    bool isPointInsideObject(const glm::vec3& p, Object* obj, glm::vec3& normal, float& penetration);
};

#endif
