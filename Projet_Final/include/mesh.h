// Mesh class for managing vertex data and rendering geometry
#ifndef MESH_H
#define MESH_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

// Vertex structure containing position, normal, and texture coordinates
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 color;
};

class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices);
    void draw();
    void cleanup();
    virtual ~Mesh() {}

    void computeBoundingSphere(glm::vec3 &center, float &radius) const;
    void computeAABB(glm::vec3 &min, glm::vec3 &max) const;
    void recomputeNormals();
    void recomputeUVs();
    void updateBuffers();
    void addPlan(float square_half_side = 1.0f);
    void addCube(float size = 1.0f);
    void subdivideLinear();
    void subdivideLoop();

    static bool loadOFF(const std::string &filename, Mesh &mesh);

private:
    unsigned int VAO, VBO, EBO;
protected:
    void setupMesh();
};

class Icosahedron : public Mesh {
public:
    Icosahedron(float radius = 1.0f);
    static Mesh* createIcosphere(float radius, unsigned int subdivisions);
private:
    void buildIcosahedron(float radius);
};

#endif // MESH_H
