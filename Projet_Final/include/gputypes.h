#ifndef GPUTYPES_H
#define GPUTYPES_H

#include <glm/glm.hpp>

struct GPUTriangle {
    glm::vec4 v0;
    glm::vec4 v1;
    glm::vec4 v2;
    glm::vec4 color;
    glm::vec4 material;
};

struct GPUObject {
    glm::vec4 bmin; // w is type (0: mesh, 1: sphere)
    glm::vec4 bmax; // w is triangle_start
    int triangle_count;
    float radius;
    float padding[2];
};

#endif // GPUTYPES_H
