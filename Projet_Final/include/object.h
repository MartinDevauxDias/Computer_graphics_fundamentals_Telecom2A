#ifndef OBJECT_H
#define OBJECT_H

#include "mesh.h"
#include "gputypes.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>

class Shader;

class Material {
public:
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;
    float reflectivity;
    float roughness;
    float transparency;
    float ior;

    unsigned int diffuseTexture;
    unsigned int specularTexture;

    Material();
    Material(const std::string& diffusePath);
    ~Material();

    void use(Shader &shader);
    void cleanup();
    static unsigned int loadTexture(const std::string &path);
};

class Object {
public:
    Mesh* mesh;
    Material* material;

    glm::vec3 position;
    glm::quat orientation;
    glm::vec3 scale;

    glm::vec3 velocity;
    glm::vec3 linearMomentum;
    float mass;
    float collisionRadius;
    bool fixedObject;
    bool isSphere = false;

    glm::vec3 angularVelocity;
    glm::vec3 angularMomentum;
    glm::mat3 inverseInertiaTensorBody;
    glm::mat3 inverseInertiaTensorWorld;

    float restitution;
    float friction;
    float drag;

    Object(Mesh* mesh, Material* material);

    void setAsBox(float width, float height, float depth, float density);
    void setAsSphere(float radius, float density);

    void draw(Shader& shader);
    glm::mat4 getModelMatrix() const;
    void setPosition(const glm::vec3& pos);
    void setRotation(const glm::vec3& eulerAngles);
    void setScale(const glm::vec3& s);

    void getAABB(glm::vec3 &min, glm::vec3 &max) const;

    void update(float dt);
    void applyForce(const glm::vec3& force);
    void applyForce(const glm::vec3& force, const glm::vec3& worldPoint);
    void applyImpulse(const glm::vec3& impulse, const glm::vec3& worldPoint);
    void applyTorque(const glm::vec3& torque);
    void applyForceAtPoint(const glm::vec3& force, const glm::vec3& worldPoint);
    void resetForces();

    void toGPU(struct GPUObject& gpuObject, std::vector<struct GPUTriangle>& gpuTriangles, size_t triangle_offset) const;

private:
    glm::vec3 netForce;
    glm::vec3 netTorque;
};

#endif // OBJECT_H
