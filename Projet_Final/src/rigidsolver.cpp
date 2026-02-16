#include "rigidsolver.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

RigidSolver::RigidSolver(glm::vec3 gravity, float floorY)
    : gravity(gravity), floorY(floorY) {}

void RigidSolver::addObject(Object* object) {
    if (object) objects.push_back(object);
}

struct OBB {
    glm::vec3 center;
    glm::vec3 axes[3];
    glm::vec3 halfExtents;
};

static OBB getOBB(Object* obj) {
    OBB obb;
    obb.center = obj->position;
    glm::mat3 R = glm::toMat3(obj->orientation);
    obb.axes[0] = R[0]; obb.axes[1] = R[1]; obb.axes[2] = R[2];
    obb.halfExtents = obj->scale * 0.5f;
    return obb;
}

void RigidSolver::step(float deltaTime) {
    // 1. Integrate Forces (Velocity)
    for (auto* obj : objects) {
        if (obj->fixedObject) continue;
        obj->velocity += gravity * deltaTime; 
        
        // Damping
        obj->velocity *= 0.999f;
        obj->angularVelocity *= 0.99f;
    }

    // 2. Collision Detection
    constraints.clear();
    detectCollisions();

    // 3. Solve (PGS)
    solve(deltaTime);

    // 4. Integrate Position
    for (auto* obj : objects) {
        if (obj->fixedObject) continue;
        obj->position += obj->velocity * deltaTime;
        
        glm::quat qOmega(0.0f, obj->angularVelocity.x, obj->angularVelocity.y, obj->angularVelocity.z);
        obj->orientation += 0.5f * deltaTime * qOmega * obj->orientation;
        obj->orientation = glm::normalize(obj->orientation);
        
        // Update Inertia Tensor and Momentum
        glm::mat3 rotationMat = glm::toMat3(obj->orientation);
        obj->inverseInertiaTensorWorld = rotationMat * obj->inverseInertiaTensorBody * glm::transpose(rotationMat);
        obj->linearMomentum = obj->velocity * obj->mass;
        obj->angularMomentum = glm::inverse(obj->inverseInertiaTensorWorld) * obj->angularVelocity;
    }
}

void RigidSolver::solve(float dt) {
    // Pre-Step
    for (auto& c : constraints) {
        float invMA = c.objA->fixedObject ? 0.0f : 1.0f / c.objA->mass;
        float invMB = (c.objB == nullptr || c.objB->fixedObject) ? 0.0f : 1.0f / c.objB->mass;
        glm::mat3 invIA = c.objA->inverseInertiaTensorWorld;
        glm::mat3 invIB = (c.objB == nullptr || c.objB->fixedObject) ? glm::mat3(0.0f) : c.objB->inverseInertiaTensorWorld;

        c.rA = c.contactPoint - c.objA->position;
        c.rB = (c.objB) ? (c.contactPoint - c.objB->position) : glm::vec3(0.0f);

        // K = JM^-1J^T (Normal)
        glm::vec3 raCn = glm::cross(c.rA, c.normal);
        glm::vec3 rbCn = glm::cross(c.rB, c.normal);
        float K = invMA + invMB + glm::dot(raCn, invIA * raCn) + glm::dot(rbCn, invIB * rbCn);
        c.massNormal = (K > 0.0f) ? 1.0f / K : 0.0f;

        // Find tangents
        if (std::abs(c.normal.x) >= 0.577f) c.tangent1 = glm::normalize(glm::vec3(c.normal.y, -c.normal.x, 0.0f));
        else c.tangent1 = glm::normalize(glm::vec3(0.0f, c.normal.z, -c.normal.y));
        c.tangent2 = glm::cross(c.normal, c.tangent1);
        
        auto calcKt = [&](const glm::vec3& t) {
            glm::vec3 raCt = glm::cross(c.rA, t);
            glm::vec3 rbCt = glm::cross(c.rB, t);
            float Kt = invMA + invMB + glm::dot(raCt, invIA * raCt) + glm::dot(rbCt, invIB * rbCt);
            return (Kt > 0.0f) ? 1.0f / Kt : 0.0f;
        };
        c.massTangent1 = calcKt(c.tangent1);
        c.massTangent2 = calcKt(c.tangent2);

        // Bias
        float beta = 0.2f;
        float slop = 0.01f;
        float restitution = (c.objB) ? std::min(c.objA->restitution, c.objB->restitution) : c.objA->restitution;
        
        glm::vec3 vA = c.objA->velocity + glm::cross(c.objA->angularVelocity, c.rA);
        glm::vec3 vB = (c.objB) ? (c.objB->velocity + glm::cross(c.objB->angularVelocity, c.rB)) : glm::vec3(0.0f);
        float vRel = glm::dot(c.normal, vA - vB);
        
        c.bias = (beta / dt) * std::max(0.0f, c.penetration - slop);
        if (vRel < -1.0f) c.bias += -restitution * vRel;
        
        c.impulseSum = 0.0f;
        c.impulseTangent1 = 0.0f;
        c.impulseTangent2 = 0.0f;
    }

    // Solve
    for (int i = 0; i < 15; ++i) {
        for (auto& c : constraints) {
            float invMA = c.objA->fixedObject ? 0.0f : 1.0f / c.objA->mass;
            float invMB = (c.objB == nullptr || c.objB->fixedObject) ? 0.0f : 1.0f / c.objB->mass;
            glm::mat3 invIA = c.objA->inverseInertiaTensorWorld;
            glm::mat3 invIB = (c.objB == nullptr || c.objB->fixedObject) ? glm::mat3(0.0f) : c.objB->inverseInertiaTensorWorld;

            // Normal
            glm::vec3 vA = c.objA->velocity + glm::cross(c.objA->angularVelocity, c.rA);
            glm::vec3 vB = (c.objB) ? (c.objB->velocity + glm::cross(c.objB->angularVelocity, c.rB)) : glm::vec3(0.0f);
            float vRel = glm::dot(c.normal, vA - vB);
            
            float lambda = c.massNormal * (c.bias - vRel);
            float oldImpulse = c.impulseSum;
            c.impulseSum = std::max(0.0f, oldImpulse + lambda);
            lambda = c.impulseSum - oldImpulse;
            
            glm::vec3 P = lambda * c.normal;
            c.objA->velocity += invMA * P;
            c.objA->angularVelocity += invIA * glm::cross(c.rA, P);
            if (c.objB && !c.objB->fixedObject) {
                c.objB->velocity -= invMB * P;
                c.objB->angularVelocity -= invIB * glm::cross(c.rB, P);
            }

            // Friction
            float mu = (c.objB) ? (c.objA->friction + c.objB->friction) * 0.5f : c.objA->friction;
            auto solveT = [&](glm::vec3& t, float& massT, float& impulseT) {
                glm::vec3 vA_t = c.objA->velocity + glm::cross(c.objA->angularVelocity, c.rA);
                glm::vec3 vB_t = (c.objB) ? (c.objB->velocity + glm::cross(c.objB->angularVelocity, c.rB)) : glm::vec3(0.0f);
                float vt = glm::dot(t, vA_t - vB_t);
                float lambdaT = -massT * vt;
                
                float maxF = mu * c.impulseSum;
                float oldT = impulseT;
                impulseT = std::max(-maxF, std::min(maxF, oldT + lambdaT));
                lambdaT = impulseT - oldT;
                
                glm::vec3 PT = lambdaT * t;
                c.objA->velocity += invMA * PT;
                c.objA->angularVelocity += invIA * glm::cross(c.rA, PT);
                if (c.objB && !c.objB->fixedObject) {
                    c.objB->velocity -= invMB * PT;
                    c.objB->angularVelocity -= invIB * glm::cross(c.rB, PT);
                }
            };
            solveT(c.tangent1, c.massTangent1, c.impulseTangent1);
            solveT(c.tangent2, c.massTangent2, c.impulseTangent2);
        }
    }
}

void RigidSolver::detectCollisions() {
    for (auto* obj : objects) {
        if (obj->fixedObject) continue;
        glm::mat3 R = glm::toMat3(obj->orientation);
        if (obj->collisionRadius > 0.0f) {
            float r = obj->collisionRadius * obj->scale.y;
            if (obj->position.y - r < floorY) {
                constraints.push_back({obj, nullptr, obj->position + glm::vec3(0,-r,0), glm::vec3(0,1,0), floorY - (obj->position.y - r)});
            }
        } else {
            for (const auto& v : obj->mesh->vertices) {
                glm::vec3 p = obj->position + R * (v.position * obj->scale);
                if (p.y < floorY) constraints.push_back({obj, nullptr, p, glm::vec3(0,1,0), floorY - p.y});
            }
        }
    }

    for (size_t i = 0; i < objects.size(); ++i) {
        for (size_t j = i + 1; j < objects.size(); ++j) {
            Object *A = objects[i], *B = objects[j];
            if (A->fixedObject && B->fixedObject) continue;
            OBB obbA = getOBB(A), obbB = getOBB(B);
            float minP = 1e10f; glm::vec3 axis;
            auto check = [&](glm::vec3 a) {
                if (glm::length(a) < 0.001f) return true;
                a = glm::normalize(a);
                float ra = obbA.halfExtents.x * std::abs(glm::dot(a, obbA.axes[0])) + obbA.halfExtents.y * std::abs(glm::dot(a, obbA.axes[1])) + obbA.halfExtents.z * std::abs(glm::dot(a, obbA.axes[2]));
                float rb = obbB.halfExtents.x * std::abs(glm::dot(a, obbB.axes[0])) + obbB.halfExtents.y * std::abs(glm::dot(a, obbB.axes[1])) + obbB.halfExtents.z * std::abs(glm::dot(a, obbB.axes[2]));
                float d = glm::dot(obbB.center - obbA.center, a);
                float o = ra + rb - std::abs(d);
                if (o < 0) return false;
                if (o < minP) { minP = o; axis = (d > 0) ? -a : a; }
                return true;
            };
            bool hit = check(obbA.axes[0]) && check(obbA.axes[1]) && check(obbA.axes[2]) && check(obbB.axes[0]) && check(obbB.axes[1]) && check(obbB.axes[2]);
            if (hit) { for(int x=0; x<3; ++x) for(int y=0; y<3; ++y) if(!check(glm::cross(obbA.axes[x], obbB.axes[y]))) { hit = false; break; } }
            if (hit) {
                // SAT normal 'axis' points from B towards A
                auto sample = [&](Object* s, Object* t, glm::vec3 n, bool isS_A) {
                    glm::mat3 Rs = glm::toMat3(s->orientation);
                    for(const auto& v : s->mesh->vertices) {
                        glm::vec3 p = s->position + Rs * (v.position * s->scale);
                        glm::vec3 dummyN; float pen;
                        if(isPointInsideObject(p, t, dummyN, pen)) {
                            // Calculate specific penetration for this point along 'axis'
                            // axis points from B to A.
                            // For a point in A, penetration is distance it's buried in B.
                            // For a point in B, penetration is distance it's buried in A.
                            // Actually, minP is a good estimate, but let's try to be precise if we can.
                            // But minP is safer for SAT.
                            if (isS_A) constraints.push_back({A, B, p, axis, pen});
                            else       constraints.push_back({A, B, p, axis, pen}); 
                        }
                    }
                };
                
                size_t prevCount = constraints.size();
                sample(A, B, axis, true);
                sample(B, A, axis, false);

                if (constraints.size() == prevCount) {
                    // Fallback to center point if no vertices are "inside" (e.g. edge-edge)
                    constraints.push_back({A, B, (obbA.center + obbB.center)*0.5f, axis, minP});
                }
            }
        }
    }
}

bool RigidSolver::isPointInsideObject(const glm::vec3& p, Object* obj, glm::vec3& normal, float& penetration) {
    if (obj->collisionRadius > 0.0f) {
        float d = glm::distance(p, obj->position); float r = obj->collisionRadius * obj->scale.x;
        if (d < r) { normal = glm::normalize(p - obj->position); penetration = r - d; return true; }
        return false;
    } else {
        glm::mat3 R_inv = glm::transpose(glm::toMat3(obj->orientation));
        glm::vec3 pL = R_inv * (p - obj->position);
        glm::vec3 h = obj->scale * 0.5f;
        if (std::abs(pL.x) <= h.x && std::abs(pL.y) <= h.y && std::abs(pL.z) <= h.z) {
            float dx = h.x - std::abs(pL.x); float dy = h.y - std::abs(pL.y); float dz = h.z - std::abs(pL.z);
            if (dx < dy && dx < dz) { normal = glm::toMat3(obj->orientation) * glm::vec3(pL.x > 0 ? 1 : -1, 0, 0); penetration = dx; }
            else if (dy < dz) { normal = glm::toMat3(obj->orientation) * glm::vec3(0, pL.y > 0 ? 1 : -1, 0); penetration = dy; }
            else { normal = glm::toMat3(obj->orientation) * glm::vec3(0, 0, pL.z > 0 ? 1 : -1); penetration = dz; }
            return true;
        }
    }
    return false;
}

void RigidSolver::reset() {}
