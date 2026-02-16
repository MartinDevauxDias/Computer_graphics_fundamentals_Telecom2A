#include "mesh.h"
#include <map>
#include <algorithm>
#include <cfloat>
#include <iostream>
#include <fstream>
#include <sstream>

// Constructor: create mesh from vertex and index data
Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices)
    : vertices(vertices), indices(indices), VAO(0), VBO(0), EBO(0)
{
    setupMesh();
}

// Initialize GPU buffers and configure vertex attributes
void Mesh::setupMesh()
{
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
    if (EBO != 0) glDeleteBuffers(1, &EBO);

    // Generate buffers
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    // Configure vertex attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);                          // Positions
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, normal));   // Normals
    glEnableVertexAttribArray(1);
    
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, texCoords)); // Texture coords
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, color)); // Color
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
}

// Render the mesh
void Mesh::draw()
{
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// Release GPU resources
void Mesh::cleanup()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void Mesh::updateBuffers()
{
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), &vertices[0]);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(unsigned int), &indices[0]);
}

void Mesh::computeBoundingSphere(glm::vec3 &center, float &radius) const {
    center = glm::vec3(0.0f);
    if (vertices.empty()) {
        radius = 0.0f;
        return;
    }
    for(const auto &v : vertices) center += v.position;
    center /= (float)vertices.size();
    radius = 0.0f;
    for(const auto &v : vertices) 
        radius = std::max(radius, glm::distance(center, v.position));
}

void Mesh::recomputeNormals() {
    // Reset all vertex normals
    for(auto &v : vertices) v.normal = glm::vec3(0.0f);

    // Calculate face normals and accumulate
    for(size_t i = 0; i < indices.size(); i += 3) {
        Vertex &v1 = vertices[indices[i]];
        Vertex &v2 = vertices[indices[i+1]];
        Vertex &v3 = vertices[indices[i+2]];

        glm::vec3 edge1 = v2.position - v1.position;
        glm::vec3 edge2 = v3.position - v1.position;
        glm::vec3 faceNormal = glm::cross(edge1, edge2);

        v1.normal += faceNormal;
        v2.normal += faceNormal;
        v3.normal += faceNormal;
    }

    // Normalize
    for(auto &v : vertices) {
        if (glm::length(v.normal) > 1e-6f)
            v.normal = glm::normalize(v.normal);
    }
}
void Mesh::recomputeUVs() {
    if (vertices.empty()) return;

    float xMin = FLT_MAX, xMax = -FLT_MAX;
    float yMin = FLT_MAX, yMax = -FLT_MAX;

    for (const auto &v : vertices) {
        xMin = std::min(xMin, v.position.x);
        xMax = std::max(xMax, v.position.x);
        yMin = std::min(yMin, v.position.y);
        yMax = std::max(yMax, v.position.y);
    }

    float xRange = xMax - xMin;
    float yRange = yMax - yMin;

    for (auto &v : vertices) {
        v.texCoords = glm::vec2(
            xRange > 1e-6f ? (v.position.x - xMin) / xRange : 0.5f,
            yRange > 1e-6f ? (v.position.y - yMin) / yRange : 0.5f
        );
    }
}

void Mesh::addPlan(float square_half_side) {
    unsigned int startIdx = (unsigned int)vertices.size();

    Vertex v1, v2, v3, v4;
    v1.position = glm::vec3(-square_half_side, 0.0f, -square_half_side);
    v2.position = glm::vec3( square_half_side, 0.0f, -square_half_side);
    v3.position = glm::vec3( square_half_side, 0.0f,  square_half_side);
    v4.position = glm::vec3(-square_half_side, 0.0f,  square_half_side);

    v1.texCoords = glm::vec2(0.0f, 0.0f);
    v2.texCoords = glm::vec2(1.0f, 0.0f);
    v3.texCoords = glm::vec2(1.0f, 1.0f);
    v4.texCoords = glm::vec2(0.0f, 1.0f);

    glm::vec3 n = glm::vec3(0.0f, 1.0f, 0.0f);
    v1.normal = v2.normal = v3.normal = v4.normal = n;

    glm::vec3 white = glm::vec3(1.0f);
    v1.color = v2.color = v3.color = v4.color = white;

    vertices.push_back(v1); vertices.push_back(v2);
    vertices.push_back(v3); vertices.push_back(v4);

    indices.push_back(startIdx + 0); indices.push_back(startIdx + 1); indices.push_back(startIdx + 2);
    indices.push_back(startIdx + 0); indices.push_back(startIdx + 2); indices.push_back(startIdx + 3);

    setupMesh();
}

void Mesh::addCube(float size) {
    float s = size / 2.0f;
    unsigned int startIdx = (unsigned int)vertices.size();

    // 24 vertices for a cube (6 faces * 4 vertices) for proper normals
    struct CubeFace { glm::vec3 pos[4]; glm::vec3 normal; };
    CubeFace faces[6] = {
        {{{-s,-s, s}, { s,-s, s}, { s, s, s}, {-s, s, s}}, { 0, 0, 1}}, // Front
        {{{ s,-s,-s}, {-s,-s,-s}, {-s, s,-s}, { s, s,-s}}, { 0, 0,-1}}, // Back
        {{{-s, s, s}, { s, s, s}, { s, s,-s}, {-s, s,-s}}, { 0, 1, 0}}, // Top
        {{{-s,-s,-s}, { s,-s,-s}, { s,-s, s}, {-s,-s, s}}, { 0,-1, 0}}, // Bottom
        {{{-s,-s,-s}, {-s,-s, s}, {-s, s, s}, {-s, s,-s}}, {-1, 0, 0}}, // Left
        {{{ s,-s, s}, { s,-s,-s}, { s, s,-s}, { s, s, s}}, { 1, 0, 0}}  // Right
    };

    for(int i=0; i<6; ++i) {
        unsigned int faceStart = (unsigned int)vertices.size();
        for(int j=0; j<4; ++j) {
            Vertex v;
            v.position = faces[i].pos[j];
            v.normal = faces[i].normal;
            v.texCoords = (j==0)?glm::vec2(0,0):(j==1)?glm::vec2(1,0):(j==2)?glm::vec2(1,1):glm::vec2(0,1);
            v.color = glm::vec3(1.0f);
            vertices.push_back(v);
        }
        indices.push_back(faceStart + 0); indices.push_back(faceStart + 1); indices.push_back(faceStart + 2);
        indices.push_back(faceStart + 0); indices.push_back(faceStart + 2); indices.push_back(faceStart + 3);
    }

    setupMesh();
}

void Mesh::subdivideLinear() {
    std::vector<Vertex> newVertices = vertices;
    std::vector<unsigned int> newIndices;

    struct Edge {
        unsigned int a, b;
        Edge(unsigned int c, unsigned int d) : a(std::min(c, d)), b(std::max(c, d)) {}
        bool operator<(const Edge& o) const { return a < o.a || (a == o.a && b < o.b); }
    };

    std::map<Edge, unsigned int> newVertexOnEdge;

    for (size_t i = 0; i < indices.size(); i += 3) {
        unsigned int v[3] = {indices[i], indices[i+1], indices[i+2]};
        unsigned int mid[3];

        for (int j = 0; j < 3; ++j) {
            Edge e(v[j], v[(j+1)%3]);
            auto it = newVertexOnEdge.find(e);
            if (it == newVertexOnEdge.end()) {
                const Vertex &v1 = vertices[e.a];
                const Vertex &v2 = vertices[e.b];
                Vertex vm;
                vm.position = (v1.position + v2.position) * 0.5f;
                vm.normal = glm::normalize(v1.normal + v2.normal);
                vm.texCoords = (v1.texCoords + v2.texCoords) * 0.5f;
                vm.color = (v1.color + v2.color) * 0.5f;
                
                mid[j] = (unsigned int)newVertices.size();
                newVertices.push_back(vm);
                newVertexOnEdge[e] = mid[j];
            } else {
                mid[j] = it->second;
            }
        }

        newIndices.push_back(v[0]);   newIndices.push_back(mid[0]); newIndices.push_back(mid[2]);
        newIndices.push_back(mid[0]); newIndices.push_back(v[1]);   newIndices.push_back(mid[1]);
        newIndices.push_back(mid[2]); newIndices.push_back(mid[1]); newIndices.push_back(v[2]);
        newIndices.push_back(mid[0]); newIndices.push_back(mid[1]); newIndices.push_back(mid[2]);
    }

    vertices = newVertices;
    indices = newIndices;
    setupMesh();
}

bool Mesh::loadOFF(const std::string &filename, Mesh &mesh) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return false;
    }

    std::string header;
    file >> header;
    if (header != "OFF") {
        // Some OFF files have "OFF" and vertex count on the same line, or no "OFF" header
        if (header.find("OFF") == std::string::npos) {
             std::cerr << "Error: Invalid OFF file " << filename << std::endl;
             return false;
        }
    }

    int numVertices, numFaces, numEdges;
    if (!(file >> numVertices >> numFaces >> numEdges)) {
        // Handle cases where OFF is followed by numbers immediately
        std::stringstream ss(header.substr(3));
        ss >> numVertices;
        file >> numFaces >> numEdges;
    }

    mesh.vertices.clear();
    mesh.indices.clear();

    for (int i = 0; i < numVertices; ++i) {
        Vertex v;
        file >> v.position.x >> v.position.y >> v.position.z;
        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v.texCoords = glm::vec2(0.0f);
        v.color = glm::vec3(1.0f);
        mesh.vertices.push_back(v);
    }

    for (int i = 0; i < numFaces; ++i) {
        int n;
        file >> n;
        if (n == 3) {
            unsigned int a, b, c;
            file >> a >> b >> c;
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
        } else if (n > 3) {
            std::vector<unsigned int> face(n);
            for (int j = 0; j < n; ++j) file >> face[j];
            for (int j = 1; j < n - 1; ++j) {
                mesh.indices.push_back(face[0]);
                mesh.indices.push_back(face[j]);
                mesh.indices.push_back(face[j+1]);
            }
        }
    }

    mesh.recomputeNormals();
    mesh.recomputeUVs();
    mesh.setupMesh();
    return true;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Mesh::subdivideLoop() {
    if (indices.empty()) return;

    struct Edge {
        unsigned int a, b;
        Edge(unsigned int c, unsigned int d) : a(std::min(c, d)), b(std::max(c, d)) {}
        bool operator<(const Edge &o) const { return a < o.a || (a == o.a && b < o.b); }
    };

    std::vector<std::vector<unsigned int>> vertexNeighbors(vertices.size());
    std::map<Edge, std::vector<unsigned int>> edgeOppositeVertices;
    std::map<Edge, unsigned int> newVertexOnEdge;

    // Topology Analysis
    for (size_t i = 0; i < indices.size(); i += 3) {
        unsigned int v[3] = {indices[i], indices[i+1], indices[i+2]};
        for (int j = 0; j < 3; ++j) {
            unsigned int v0 = v[j];
            unsigned int v1 = v[(j + 1) % 3];
            unsigned int v2 = v[(j + 2) % 3];
            vertexNeighbors[v0].push_back(v1);
            edgeOppositeVertices[Edge(v0, v1)].push_back(v2);
        }
    }

    // New Positions for Even (Original) Vertices
    std::vector<Vertex> newVertices;
    for (unsigned int i = 0; i < vertices.size(); ++i) {
        std::sort(vertexNeighbors[i].begin(), vertexNeighbors[i].end());
        vertexNeighbors[i].erase(std::unique(vertexNeighbors[i].begin(), vertexNeighbors[i].end()), vertexNeighbors[i].end());

        std::vector<unsigned int> boundaryNeighbors;
        for (unsigned int nIdx : vertexNeighbors[i]) {
            if (edgeOppositeVertices[Edge(i, nIdx)].size() == 1) {
                boundaryNeighbors.push_back(nIdx);
            }
        }

        Vertex vNew = vertices[i];
        if (boundaryNeighbors.size() == 2) {
            // Boundary Rule
            vNew.position = 0.75f * vertices[i].position + 0.125f * (vertices[boundaryNeighbors[0]].position + vertices[boundaryNeighbors[1]].position);
        } else {
            // Interior Rule
            int n = (int)vertexNeighbors[i].size();
            if (n > 2) {
                float alpha;
                if (n == 6) {
                    alpha = 5.0f / 8.0f;
                } else {
                    float term = 3.0f / 8.0f + 0.25f * cosf(2.0f * (float)M_PI / n);
                    alpha = 3.0f / 8.0f + term * term;
                }
                float beta = (1.0f - alpha) / n;
                glm::vec3 neighborSum(0.0f);
                for (unsigned int nIdx : vertexNeighbors[i]) neighborSum += vertices[nIdx].position;
                vNew.position = alpha * vertices[i].position + beta * neighborSum;
            }
        }
        newVertices.push_back(vNew);
    }

    // New (Odd) Vertices for Edges and build new triangles
    std::vector<unsigned int> newIndices;
    for (size_t i = 0; i < indices.size(); i += 3) {
        unsigned int v[3] = {indices[i], indices[i+1], indices[i+2]};
        unsigned int oddIdx[3];
        Edge edges[3] = {Edge(v[0], v[1]), Edge(v[1], v[2]), Edge(v[2], v[0])};

        for (int j = 0; j < 3; ++j) {
            auto it = newVertexOnEdge.find(edges[j]);
            if (it == newVertexOnEdge.end()) {
                unsigned int v1Idx = edges[j].a;
                unsigned int v2Idx = edges[j].b;
                const auto &opposites = edgeOppositeVertices[edges[j]];

                Vertex vOdd;
                if (opposites.size() == 2) { // Interior Edge
                    unsigned int v3Idx = opposites[0];
                    unsigned int v4Idx = opposites[1];
                    vOdd.position = 0.375f * (vertices[v1Idx].position + vertices[v2Idx].position) + 
                                   0.125f * (vertices[v3Idx].position + vertices[v4Idx].position);
                    vOdd.color = 0.5f * (vertices[v1Idx].color + vertices[v2Idx].color);
                    vOdd.texCoords = 0.5f * (vertices[v1Idx].texCoords + vertices[v2Idx].texCoords);
                } else { // Boundary Edge
                    vOdd.position = 0.5f * (vertices[v1Idx].position + vertices[v2Idx].position);
                    vOdd.color = 0.5f * (vertices[v1Idx].color + vertices[v2Idx].color);
                    vOdd.texCoords = 0.5f * (vertices[v1Idx].texCoords + vertices[v2Idx].texCoords);
                }
                oddIdx[j] = (unsigned int)newVertices.size();
                newVertices.push_back(vOdd);
                newVertexOnEdge[edges[j]] = oddIdx[j];
            } else {
                oddIdx[j] = it->second;
            }
        }

        newIndices.push_back(v[0]);      newIndices.push_back(oddIdx[0]); newIndices.push_back(oddIdx[2]);
        newIndices.push_back(oddIdx[0]); newIndices.push_back(v[1]);      newIndices.push_back(oddIdx[1]);
        newIndices.push_back(oddIdx[2]); newIndices.push_back(oddIdx[1]); newIndices.push_back(v[2]);
        newIndices.push_back(oddIdx[0]); newIndices.push_back(oddIdx[1]); newIndices.push_back(oddIdx[2]);
    }

    vertices = newVertices;
    indices = newIndices;
    recomputeNormals();
    setupMesh();
}

void Mesh::computeAABB(glm::vec3 &min, glm::vec3 &max) const {
    if (vertices.empty()) {
        min = max = glm::vec3(0.0f);
        return;
    }
    min = max = vertices[0].position;
    for (const auto &v : vertices) {
        min = glm::min(min, v.position);
        max = glm::max(max, v.position);
    }
}
