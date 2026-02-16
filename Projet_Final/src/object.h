// Object class representing a renderable entity with mesh, material, and transform
#ifndef OBJECT_H
#define OBJECT_H

#include "mesh.h"
#include "material.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class Object {
public:
    Mesh* mesh;
    Material* material;

    // Transform properties
    glm::vec3 position;
    glm::quat orientation; // Using Quaternions for stable rotation
    glm::vec3 scale;

    // Physics properties - Linear
    glm::vec3 velocity;
    glm::vec3 linearMomentum;
    float mass;
    float collisionRadius; // For sphere-based collision logic
    bool fixedObject;     // If true, object does not move

    // Physics properties - Rotational
    glm::vec3 angularVelocity;
    glm::vec3 angularMomentum;
    glm::mat3 inverseInertiaTensorBody;  // Constant in local space
    glm::mat3 inverseInertiaTensorWorld; // Updates every frame

    // Material properties
    float restitution;             // Bounciness (0 = no bounce, 1 = perfect bounce)
    float friction;                // Surface friction coefficient
    float drag;                    // Air resistance

    // Constructor
    Object(Mesh* mesh, Material* material);

    // Physics initialization helpers
    void setAsBox(float width, float height, float depth, float density);
    void setAsSphere(float radius, float density);

    // Render the object
    void draw(class Shader& shader);

    // Get World-Space AABB
    void getAABB(glm::vec3 &min, glm::vec3 &max) const;

    // Get Model Matrix
    glm::mat4 getModelMatrix() const;

    // Physics methods
    void update(float deltaTime);
    void applyForce(const glm::vec3& force);
    void applyForceAtPoint(const glm::vec3& force, const glm::vec3& worldPoint);
    void applyTorque(const glm::vec3& torque);
    void resetForces();

    // Transform setters
    void setPosition(const glm::vec3& pos);
    void setRotation(const glm::vec3& eulerAngles); // Helper to set from euler
    void setScale(const glm::vec3& scl);

private:
    glm::vec3 netForce;
    glm::vec3 netTorque;
};

#endif