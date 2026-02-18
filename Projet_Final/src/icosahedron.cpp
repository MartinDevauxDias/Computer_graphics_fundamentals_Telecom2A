#include "mesh.h"
#include <cmath>
#include <map>

Icosahedron::Icosahedron(float radius)
    : Mesh(std::vector<Vertex>(), std::vector<unsigned int>())
{
    buildIcosahedron(radius);
}

void Icosahedron::buildIcosahedron(float radius) {
    const float phi = (1.0f + sqrtf(5.0f)) / 2.0f;
    
    // 12 Vertices of a regular icosahedron
    std::vector<glm::vec3> pos = {
        {-1,  phi,  0}, { 1,  phi,  0}, {-1, -phi,  0}, { 1, -phi,  0},
        { 0, -1,  phi}, { 0,  1,  phi}, { 0, -1, -phi}, { 0,  1, -phi},
        { phi,  0, -1}, { phi,  0,  1}, {-phi,  0, -1}, {-phi,  0,  1}
    };

    // 20 Faces (triangles)
    std::vector<unsigned int> faceIndices = {
        0, 11, 5,   0, 5, 1,    0, 1, 7,    0, 7, 10,   0, 10, 11,
        1, 5, 9,    5, 11, 4,   11, 10, 2,  10, 7, 6,   7, 1, 8,
        3, 9, 4,    3, 4, 2,    3, 2, 6,    3, 6, 8,    3, 8, 9,
        4, 9, 5,    2, 4, 11,   6, 2, 10,   8, 6, 7,    9, 8, 1
    };

    vertices.clear();
    indices.clear();

    for (const auto& p : pos) {
        Vertex v;
        v.position = glm::normalize(p) * radius;
        v.normal = glm::normalize(v.position);
        v.texCoords = glm::vec2(0.0f); // Could be improved with spherical mapping
        v.color = glm::vec3(1.0f);
        vertices.push_back(v);
    }

    indices = faceIndices;
    setupMesh();
}

Mesh* Icosahedron::createIcosphere(float radius, unsigned int subdivisions) {
    Icosahedron* mesh = new Icosahedron(radius);
    
    for (unsigned int i = 0; i < subdivisions; ++i) {
        mesh->subdivideLoop();
        // Project all vertices back to the sphere surface to maintain perfect radius
        for (auto& v : mesh->vertices) {
            v.position = glm::normalize(v.position) * radius;
            v.normal = glm::normalize(v.position);
        }
    }
    
    mesh->setupMesh();
    return mesh;
}
