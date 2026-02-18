#include "object.h"
#include "shader.h"
#include <cfloat>
#include <iostream>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

Object::Object(Mesh* mesh, Material* material)
    : mesh(mesh), material(material), position(0.0f), orientation(1.0f, 0.0f, 0.0f, 0.0f), scale(1.0f),
      velocity(0.0f), linearMomentum(0.0f), mass(1.0f), collisionRadius(0.0f), fixedObject(false),
      angularVelocity(0.0f), angularMomentum(0.0f), 
      inverseInertiaTensorBody(glm::mat3(1.0f)), inverseInertiaTensorWorld(glm::mat3(1.0f)),
      restitution(0.5f), friction(0.3f), drag(0.01f), netForce(0.0f), netTorque(0.0f) {}

void Object::setAsBox(float width, float height, float depth, float density) {
    mass = width * height * depth * density;
    collisionRadius = 0.0f; // Box uses vertex collision
    float Ixx = (height * height + depth * depth) * mass / 12.0f;
    float Iyy = (width * width + depth * depth) * mass / 12.0f;
    float Izz = (width * width + height * height) * mass / 12.0f;
    
    glm::mat3 I0(0.0f);
    I0[0][0] = Ixx; I0[1][1] = Iyy; I0[2][2] = Izz;
    inverseInertiaTensorBody = glm::inverse(I0);
}

void Object::setAsSphere(float radius, float density) {
    mass = (4.0f / 3.0f) * 3.14159f * radius * radius * radius * density;
    collisionRadius = radius;
    isSphere = true;
    float I = (2.0f / 5.0f) * mass * radius * radius;
    
    glm::mat3 I0(0.0f);
    I0[0][0] = I0[1][1] = I0[2][2] = I;
    inverseInertiaTensorBody = glm::inverse(I0);
}

void Object::draw(Shader& shader) {
    material->use(shader);
    glm::mat4 model = getModelMatrix();
    shader.set("model", model);
    mesh->draw();
}

void Object::setPosition(const glm::vec3& pos) { position = pos; }
void Object::setRotation(const glm::vec3& euler) { orientation = glm::quat(glm::radians(euler)); }
void Object::setScale(const glm::vec3& scl) { scale = scl; }

glm::mat4 Object::getModelMatrix() const {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = model * glm::toMat4(orientation);
    model = glm::scale(model, scale);
    return model;
}

void Object::getAABB(glm::vec3 &min, glm::vec3 &max) const {
    glm::vec3 lMin, lMax;
    mesh->computeAABB(lMin, lMax);
    glm::mat4 model = getModelMatrix();
    glm::vec3 corners[8] = {
        glm::vec3(lMin.x, lMin.y, lMin.z), glm::vec3(lMax.x, lMin.y, lMin.z),
        glm::vec3(lMin.x, lMax.y, lMin.z), glm::vec3(lMax.x, lMax.y, lMin.z),
        glm::vec3(lMin.x, lMin.y, lMax.z), glm::vec3(lMax.x, lMin.y, lMax.z),
        glm::vec3(lMin.x, lMax.y, lMax.z), glm::vec3(lMax.x, lMax.y, lMax.z)
    };
    min = glm::vec3(FLT_MAX); max = glm::vec3(-FLT_MAX);
    for (int i = 0; i < 8; i++) {
        glm::vec3 worldCorner = glm::vec3(model * glm::vec4(corners[i], 1.0f));
        min = glm::min(min, worldCorner); max = glm::max(max, worldCorner);
    }
}

void Object::update(float deltaTime) {
    if (fixedObject) { resetForces(); return; }

    // 1. Linear Integration
    linearMomentum += netForce * deltaTime;
    linearMomentum *= 0.999f; // Global linear damping
    velocity = linearMomentum / mass;
    position += velocity * deltaTime;

    // 2. Rotational Integration
    angularMomentum += netTorque * deltaTime;
    angularMomentum *= 0.99f; // Global angular damping
    glm::mat3 rotationMat = glm::toMat3(orientation);
    inverseInertiaTensorWorld = rotationMat * inverseInertiaTensorBody * glm::transpose(rotationMat);
    angularVelocity = inverseInertiaTensorWorld * angularMomentum;

    // 3. Update Orientation (Quaternion Integration)
    glm::quat qOmega(0.0f, angularVelocity.x, angularVelocity.y, angularVelocity.z);
    orientation += 0.5f * deltaTime * qOmega * orientation;
    orientation = glm::normalize(orientation);

    resetForces();
}

void Object::applyForce(const glm::vec3& force) { netForce += force; }
void Object::applyTorque(const glm::vec3& torque) { netTorque += torque; }

void Object::applyForceAtPoint(const glm::vec3& force, const glm::vec3& worldPoint) {
    netForce += force;
    glm::vec3 r = worldPoint - position;
    netTorque += glm::cross(r, force);
}

void Object::resetForces() {
    netForce = glm::vec3(0.0f);
    netTorque = glm::vec3(0.0f);
}

void Object::toGPU(GPUObject& gpuObject, std::vector<GPUTriangle>& gpuTriangles, size_t triangle_offset) const {
    glm::vec3 baseColor = this->material->diffuse;
    float reflect = this->material->reflectivity;
    float roughness = this->material->roughness;
    float transparency = this->material->transparency;
    float ior = this->material->ior;
    glm::vec4 matInfo = glm::vec4(reflect, roughness, ior, transparency);

    if (isSphere) {
        float r = scale.x; // Assume uniform scale
        glm::vec3 bmin = position - glm::vec3(r);
        glm::vec3 bmax = position + glm::vec3(r);
        
        gpuObject.bmin = glm::vec4(bmin, 1.0f); // 1.0f means sphere
        gpuObject.bmax = glm::vec4(bmax, (float)triangle_offset);
        gpuObject.triangle_count = 0;
        gpuObject.radius = r;

        // For spheres, we use one dummy triangle to store material/color/center in v0
        GPUTriangle& tri = gpuTriangles[triangle_offset];
        tri.v0 = glm::vec4(position, 1.0f);
        tri.color = glm::vec4(baseColor, 1.0f);
        tri.material = matInfo;
    } else {
        glm::mat4 model = getModelMatrix();
        size_t numTris = mesh->indices.size() / 3;
        glm::vec3 bmin(1e30f), bmax(-1e30f);

        for (size_t i = 0; i < numTris; ++i) {
            GPUTriangle& tri = gpuTriangles[triangle_offset + i];
            
            tri.v0 = model * glm::vec4(mesh->vertices[mesh->indices[i*3]].position, 1.0f);
            tri.v1 = model * glm::vec4(mesh->vertices[mesh->indices[i*3+1]].position, 1.0f);
            tri.v2 = model * glm::vec4(mesh->vertices[mesh->indices[i*3+2]].position, 1.0f);
            tri.color = glm::vec4(baseColor, 1.0f);
            tri.material = matInfo;

            bmin = glm::min(bmin, glm::vec3(tri.v0));
            bmin = glm::min(bmin, glm::vec3(tri.v1));
            bmin = glm::min(bmin, glm::vec3(tri.v2));
            
            bmax = glm::max(bmax, glm::vec3(tri.v0));
            bmax = glm::max(bmax, glm::vec3(tri.v1));
            bmax = glm::max(bmax, glm::vec3(tri.v2));
        }

        gpuObject.bmin = glm::vec4(bmin, 0.0f); // 0.0f means mesh
        gpuObject.bmax = glm::vec4(bmax, (float)triangle_offset);
        gpuObject.triangle_count = (int)numTris;
        gpuObject.radius = 0.0f;
    }
}