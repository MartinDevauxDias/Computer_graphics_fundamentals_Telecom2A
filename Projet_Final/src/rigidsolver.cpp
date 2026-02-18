#include "rigidsolver.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <omp.h>
#include <iostream>

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
    
    // 1. Integrate Forces (Velocity) - Parallelized
    #pragma omp parallel for
    for (int i = 0; i < (int)objects.size(); ++i) {
        Object* obj = objects[i];
        if (obj->fixedObject) continue;
        obj->velocity += gravity * deltaTime; 
        
        obj->velocity *= 0.999f;
        obj->angularVelocity *= 0.999f;
    }

    // 2. Collision Detection - Already Parallelized internally
    constraints.clear();
    detectCollisions();

    // 3. Solve (PGS) - Inherently serial, better on CPU for ~100 objects
    solve(deltaTime);

    // 4. Integrate Position - Parallelized
    #pragma omp parallel for
    for (int i = 0; i < (int)objects.size(); ++i) {
        Object* obj = objects[i];
        if (obj->fixedObject) continue;
        obj->position += obj->velocity * deltaTime;
        
        // Correct angular integration
        float angVelLen = glm::length(obj->angularVelocity);
        if (angVelLen > 0.0001f) {
            glm::vec3 axis = obj->angularVelocity / angVelLen;
            float angle = angVelLen * deltaTime;
            glm::quat deltaRot = glm::angleAxis(angle, axis);
            obj->orientation = glm::normalize(deltaRot * obj->orientation);
        }
        
        // Update Inertia Tensor and Momentum
        glm::mat3 rotationMat = glm::toMat3(obj->orientation);
        obj->inverseInertiaTensorWorld = rotationMat * obj->inverseInertiaTensorBody * glm::transpose(rotationMat);
        obj->linearMomentum = obj->velocity * obj->mass;
    }
}

void RigidSolver::solve(float dt) {
    const int iterations = 20;
    const float beta = 0.02f;
    const float slop = 0.01f;  

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

        // Find initial relative velocity
        glm::vec3 vA = c.objA->velocity + glm::cross(c.objA->angularVelocity, c.rA);
        glm::vec3 vB = (c.objB) ? (c.objB->velocity + glm::cross(c.objB->angularVelocity, c.rB)) : glm::vec3(0.0f);
        float vRel = glm::dot(c.normal, vA - vB);

        // Bias
        float restitution = (c.objB) ? std::min(c.objA->restitution, c.objB->restitution) : c.objA->restitution;
        c.bias = (beta / dt) * std::max(0.0f, c.penetration - slop);
        
        // Add restitution bias if objects are hitting hard
        if (vRel < -1.0f) c.bias += -restitution * vRel;
        
        c.impulseSum = 0.0f;
        c.impulseTangent1 = 0.0f;
        c.impulseTangent2 = 0.0f;
    }

    // Solve
    for (int i = 0; i < iterations; ++i) {
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
    #pragma omp parallel
    {
        std::vector<ContactConstraint> localConstraints;

        // Fast Sphere-Ground check
        #pragma omp for nowait
        for (int i = 0; i < (int)objects.size(); ++i) {
            Object* obj = objects[i];
            if (obj->fixedObject) continue;
            
            if (obj->collisionRadius > 0.0f) {
                float r = obj->collisionRadius;
                if (obj->position.y - r < floorY) {
                    localConstraints.push_back({obj, nullptr, obj->position + glm::vec3(0,-r,0), glm::vec3(0,1,0), floorY - (obj->position.y - r)});
                }
            } else {
                glm::mat3 R = glm::toMat3(obj->orientation);
                for (const auto& v : obj->mesh->vertices) {
                    glm::vec3 p = obj->position + R * (v.position * obj->scale);
                    if (p.y < floorY) localConstraints.push_back({obj, nullptr, p, glm::vec3(0,1,0), floorY - p.y});
                }
            }
        }

        // Object-to-Object checks with simplified AABB filtering
        #pragma omp for nowait
        for (int i = 0; i < (int)objects.size(); ++i) {
            for (int j = i + 1; j < (int)objects.size(); ++j) {
                Object *A = objects[i], *B = objects[j];
                if (A->fixedObject && B->fixedObject) continue;

                // Broad phase: Simple distance or AABB check
                float maxRadA = (A->collisionRadius > 0.0f) ? A->collisionRadius : glm::length(A->scale * 0.5f);
                float maxRadB = (B->collisionRadius > 0.0f) ? B->collisionRadius : glm::length(B->scale * 0.5f);
                float distSq = glm::distance2(A->position, B->position);
                float combinedGap = maxRadA + maxRadB + 0.1f;
                if (distSq > combinedGap * combinedGap) continue;

                if (A->collisionRadius > 0.0f && B->collisionRadius > 0.0f) {
                    // Optimized Sphere-Sphere
                    float rA = A->collisionRadius;
                    float rB = B->collisionRadius;
                    float d = glm::sqrt(distSq);
                    if (d < rA + rB) {
                        glm::vec3 normal = glm::normalize(B->position - A->position);
                        float penetration = (rA + rB) - d;
                        localConstraints.push_back({A, B, A->position + normal * rA, -normal, penetration});
                    }
                    continue;
                }

                // Optimized Sphere-Box
                if ((A->collisionRadius > 0.0f) != (B->collisionRadius > 0.0f)) {
                    Object* sphere = (A->collisionRadius > 0.0f) ? A : B;
                    Object* box = (A->collisionRadius > 0.0f) ? B : A;
                    
                    glm::mat3 R_inv = glm::transpose(glm::toMat3(box->orientation));
                    glm::vec3 relCenter = R_inv * (sphere->position - box->position);
                    glm::vec3 h = box->scale * 0.5f;

                    // Closest point on AABB
                    glm::vec3 closest;
                    closest.x = std::max(-h.x, std::min(h.x, relCenter.x));
                    closest.y = std::max(-h.y, std::min(h.y, relCenter.y));
                    closest.z = std::max(-h.z, std::min(h.z, relCenter.z));

                    float distSq = glm::distance2(relCenter, closest);
                    if (distSq < sphere->collisionRadius * sphere->collisionRadius) {
                        float d = std::sqrt(distSq);
                        glm::vec3 normal;
                        float penetration;
                        
                        if (d > 0.0001f) {
                            normal = glm::toMat3(box->orientation) * ((relCenter - closest) / d);
                            penetration = sphere->collisionRadius - d;
                        } else {
                            // Sphere center is inside the box
                            // Find the minimal penetration axis
                            glm::vec3 dists = h - glm::abs(relCenter);
                            if (dists.x < dists.y && dists.x < dists.z) {
                                normal = box->orientation * glm::vec3(relCenter.x > 0 ? 1 : -1, 0, 0);
                                penetration = sphere->collisionRadius + dists.x;
                            } else if (dists.y < dists.z) {
                                normal = box->orientation * glm::vec3(0, relCenter.y > 0 ? 1 : -1, 0);
                                penetration = sphere->collisionRadius + dists.y;
                            } else {
                                normal = box->orientation * glm::vec3(0, 0, relCenter.z > 0 ? 1 : -1);
                                penetration = sphere->collisionRadius + dists.z;
                            }
                        }
                        
                        // Ensure normal points from B to A (the direction the impulse will push A)
                        // 'normal' is currently Box-to-Sphere (from surface to center)
                        glm::vec3 contactPoint = box->position + glm::toMat3(box->orientation) * closest;
                        if (A == sphere) {
                            localConstraints.push_back({A, B, contactPoint, normal, penetration});
                        } else {
                            localConstraints.push_back({A, B, contactPoint, -normal, penetration});
                        }
                    }
                    continue;
                }

                // Fallback to existing SAT/Sampling for Box-Box
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
                    auto sampleLocal = [&](Object* s, Object* t, glm::vec3 n, bool isS_A, std::vector<ContactConstraint>& constraints_list) {
                        glm::mat3 Rs = glm::toMat3(s->orientation);
                        glm::mat3 Rt_inv = glm::transpose(glm::toMat3(t->orientation));
                        glm::vec3 h_t = t->scale * 0.5f;

                        for(const auto& v : s->mesh->vertices) {
                            glm::vec3 p = s->position + Rs * (v.position * s->scale);
                            glm::vec3 pL = Rt_inv * (p - t->position);
                            if (std::abs(pL.x) <= h_t.x + 0.005f && std::abs(pL.y) <= h_t.y + 0.005f && std::abs(pL.z) <= h_t.z + 0.005f) 
                            {
                                float pen; glm::vec3 dummyN;
                                if (isPointInsideObject(p, t, dummyN, pen)) {
                                    constraints_list.push_back({A, B, p, axis, pen});
                                }
                            }
                        }
                    };
                    
                    size_t prevCount = localConstraints.size();
                    sampleLocal(A, B, axis, true, localConstraints);
                    sampleLocal(B, A, axis, false, localConstraints);

                    if (localConstraints.size() == prevCount) {
                        localConstraints.push_back({A, B, (obbA.center + obbB.center)*0.5f, axis, minP});
                    }
                }
            }
        }

        #pragma omp critical
        {
            constraints.insert(constraints.end(), localConstraints.begin(), localConstraints.end());
        }
    }
}

bool RigidSolver::isPointInsideObject(const glm::vec3& p, Object* obj, glm::vec3& normal, float& penetration) {
    if (obj->collisionRadius > 0.0f) {
        float d = glm::distance(p, obj->position); float r = obj->collisionRadius;
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

