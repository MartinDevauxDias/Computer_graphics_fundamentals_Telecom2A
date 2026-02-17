#ifndef GPUTYPES_H
#define GPUTYPES_H

#include <glm/glm.hpp>

// Represents a single triangle on the GPU
struct GPUTriangle {
    glm::vec4 v0;
    glm::vec4 v1;
    glm::vec4 v2;
    glm::vec4 color;
};

// Represents an object's metadata on the GPU (AABB, material, triangle info)
struct GPUObject {
    glm::vec4 bmin;
    glm::vec4 bmax; // w: triangle start index
    int triangleCount;
    float reflectivity;
    float mattness;
    float padding;
};

#endif // GPUTYPES_H