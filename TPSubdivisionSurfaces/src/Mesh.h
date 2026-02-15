#ifndef MESH_H
#define MESH_H

#include <glad/glad.h>
#include <vector>
#include <memory>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <map>
#include <set>

class Mesh
{
public:
  virtual ~Mesh();

  const std::vector<glm::vec3> &vertexPositions() const { return _vertexPositions; }
  std::vector<glm::vec3> &vertexPositions() { return _vertexPositions; }

  const std::vector<glm::vec3> &vertexNormals() const { return _vertexNormals; }
  std::vector<glm::vec3> &vertexNormals() { return _vertexNormals; }

  const std::vector<glm::vec2> &vertexTexCoords() const { return _vertexTexCoords; }
  std::vector<glm::vec2> &vertexTexCoords() { return _vertexTexCoords; }

  const std::vector<glm::uvec3> &triangleIndices() const { return _triangleIndices; }
  std::vector<glm::uvec3> &triangleIndices() { return _triangleIndices; }

  /// Compute the parameters of a sphere which bounds the mesh
  void computeBoundingSphere(glm::vec3 &center, float &radius) const;

  void recomputePerVertexNormals(bool angleBased = false);
  void recomputePerVertexTextureCoordinates();

  void init();
  void initOldGL();
  void render();
  void clear();

  void addPlan(float square_half_side = 1.0f);

  void subdivideLinear()
  {
    std::vector<glm::vec3> newVertices = _vertexPositions;
    std::vector<glm::uvec3> newTriangles;

    struct Edge
    {
      unsigned int a, b;
      Edge(unsigned int c, unsigned int d) : a(std::min<unsigned int>(c, d)), b(std::max<unsigned int>(c, d)) {}
      bool operator<(Edge const &o) const { return a < o.a || (a == o.a && b < o.b); }
      bool operator==(Edge const &o) const { return a == o.a && b == o.b; }
    };
    std::map<Edge, unsigned int> newVertexOnEdge;
    for (unsigned int tIt = 0; tIt < _triangleIndices.size(); ++tIt)
    {
      unsigned int a = _triangleIndices[tIt][0];
      unsigned int b = _triangleIndices[tIt][1];
      unsigned int c = _triangleIndices[tIt][2];

      Edge Eab(a, b);
      unsigned int oddVertexOnEdgeEab = 0;
      if (newVertexOnEdge.find(Eab) == newVertexOnEdge.end())
      {
        newVertices.push_back((_vertexPositions[a] + _vertexPositions[b]) / 2.f);
        oddVertexOnEdgeEab = newVertices.size() - 1;
        newVertexOnEdge[Eab] = oddVertexOnEdgeEab;
      }
      else
      {
        oddVertexOnEdgeEab = newVertexOnEdge[Eab];
      }

      Edge Ebc(b, c);
      unsigned int oddVertexOnEdgeEbc = 0;
      if (newVertexOnEdge.find(Ebc) == newVertexOnEdge.end())
      {
        newVertices.push_back((_vertexPositions[b] + _vertexPositions[c]) / 2.f);
        oddVertexOnEdgeEbc = newVertices.size() - 1;
        newVertexOnEdge[Ebc] = oddVertexOnEdgeEbc;
      }
      else
      {
        oddVertexOnEdgeEbc = newVertexOnEdge[Ebc];
      }

      Edge Eca(c, a);
      unsigned int oddVertexOnEdgeEca = 0;
      if (newVertexOnEdge.find(Eca) == newVertexOnEdge.end())
      {
        newVertices.push_back((_vertexPositions[c] + _vertexPositions[a]) / 2.f);
        oddVertexOnEdgeEca = newVertices.size() - 1;
        newVertexOnEdge[Eca] = oddVertexOnEdgeEca;
      }
      else
      {
        oddVertexOnEdgeEca = newVertexOnEdge[Eca];
      }

      // set new triangles :
      newTriangles.push_back(glm::uvec3(a, oddVertexOnEdgeEab, oddVertexOnEdgeEca));
      newTriangles.push_back(glm::uvec3(oddVertexOnEdgeEab, b, oddVertexOnEdgeEbc));
      newTriangles.push_back(glm::uvec3(oddVertexOnEdgeEca, oddVertexOnEdgeEbc, c));
      newTriangles.push_back(glm::uvec3(oddVertexOnEdgeEab, oddVertexOnEdgeEbc, oddVertexOnEdgeEca));
    }

    // after that:
    _triangleIndices = newTriangles;
    _vertexPositions = newVertices;
    recomputePerVertexNormals();
    recomputePerVertexTextureCoordinates();
  }

  void subdivideLoopNew()
  {
    // Declare new vertices and new triangles. Initialize the new positions for the even vertices with (0,0,0):
    std::vector<glm::vec3> newVertices(_vertexPositions.size(), glm::vec3(0, 0, 0));
    std::vector<glm::uvec3> newTriangles;

    struct Edge
    {
      unsigned int a, b;
      Edge(unsigned int c, unsigned int d) : a(std::min<unsigned int>(c, d)), b(std::max<unsigned int>(c, d)) {}
      bool operator<(Edge const &o) const { return a < o.a || (a == o.a && b < o.b); }
      bool operator==(Edge const &o) const { return a == o.a && b == o.b; }
    };

    std::map<Edge, unsigned int> newVertexOnEdge;                                     // this will be useful to find out whether we already inserted an odd vertex or not
    std::map<Edge, std::set<unsigned int>> trianglesOnEdge;                           // this will be useful to find out if an edge is boundary or not
    std::vector<std::set<unsigned int>> neighboringVertices(_vertexPositions.size()); // this will be used to store the adjacent vertices, i.e., neighboringVertices[i] will be the list of vertices that are adjacent to vertex i.
    std::vector<bool> evenVertexIsBoundary(_vertexPositions.size(), false);

    // I) First, compute the valences of the even vertices, the neighboring vertices required to update the position of the even vertices, and the boundaries:
    for (unsigned int tIt = 0; tIt < _triangleIndices.size(); ++tIt)
    {
      unsigned int a = _triangleIndices[tIt][0];
      unsigned int b = _triangleIndices[tIt][1];
      unsigned int c = _triangleIndices[tIt][2];

      // TODO: Remember the faces shared by the edge

      neighboringVertices[a].insert(b);
      neighboringVertices[a].insert(c);
      neighboringVertices[b].insert(a);
      neighboringVertices[b].insert(c);
      neighboringVertices[c].insert(a);
      neighboringVertices[c].insert(b);
    }

    // The valence of a vertex is the number of adjacent vertices:
    std::vector<unsigned int> evenVertexValence(_vertexPositions.size(), 0);
    for (unsigned int v = 0; v < _vertexPositions.size(); ++v)
    {
      evenVertexValence[v] = neighboringVertices[v].size();
    }
    // TODO: Identify even vertices (clue: check the number of triangles) and remember immediate neighbors for further calculation
    //  II) Then, compute the positions for the even vertices: (make sure that you handle the boundaries correctly)
    for (unsigned int v = 0; v < _vertexPositions.size(); ++v)
    {
      // TODO: Compute the coordinates for even vertices - check both the cases - ordinary and extraordinary
    }

    // III) Then, compute the odd vertices:
    for (unsigned int tIt = 0; tIt < _triangleIndices.size(); ++tIt)
    {
      unsigned int a = _triangleIndices[tIt][0];
      unsigned int b = _triangleIndices[tIt][1];
      unsigned int c = _triangleIndices[tIt][2];

      Edge Eab(a, b);
      unsigned int oddVertexOnEdgeEab = 0;
      if (newVertexOnEdge.find(Eab) == newVertexOnEdge.end())
      {
        newVertices.push_back(glm::vec3(0, 0, 0));
        oddVertexOnEdgeEab = newVertices.size() - 1;
        newVertexOnEdge[Eab] = oddVertexOnEdgeEab;
      }
      else
      {
        oddVertexOnEdgeEab = newVertexOnEdge[Eab];
      }

      // TODO: Update odd vertices

      Edge Ebc(b, c);
      unsigned int oddVertexOnEdgeEbc = 0;
      if (newVertexOnEdge.find(Ebc) == newVertexOnEdge.end())
      {
        newVertices.push_back(glm::vec3(0, 0, 0));
        oddVertexOnEdgeEbc = newVertices.size() - 1;
        newVertexOnEdge[Ebc] = oddVertexOnEdgeEbc;
      }
      else
      {
        oddVertexOnEdgeEbc = newVertexOnEdge[Ebc];
      }

      // TODO: Update odd vertices

      Edge Eca(c, a);
      unsigned int oddVertexOnEdgeEca = 0;
      if (newVertexOnEdge.find(Eca) == newVertexOnEdge.end())
      {
        newVertices.push_back(glm::vec3(0, 0, 0));
        oddVertexOnEdgeEca = newVertices.size() - 1;
        newVertexOnEdge[Eca] = oddVertexOnEdgeEca;
      }
      else
      {
        oddVertexOnEdgeEca = newVertexOnEdge[Eca];
      }

      // TODO: Update odd vertices

      // set new triangles :
      newTriangles.push_back(glm::uvec3(a, oddVertexOnEdgeEab, oddVertexOnEdgeEca));
      newTriangles.push_back(glm::uvec3(oddVertexOnEdgeEab, b, oddVertexOnEdgeEbc));
      newTriangles.push_back(glm::uvec3(oddVertexOnEdgeEca, oddVertexOnEdgeEbc, c));
      newTriangles.push_back(glm::uvec3(oddVertexOnEdgeEab, oddVertexOnEdgeEbc, oddVertexOnEdgeEca));
    }

    // after that:
    _triangleIndices = newTriangles;
    _vertexPositions = newVertices;
    recomputePerVertexNormals();
    recomputePerVertexTextureCoordinates();
  }

  void subdivideLoop()
  {
    // I) Data Structures Initialization
    std::vector<glm::vec3> newVertices;
    std::vector<glm::uvec3> newTriangles;

    struct Edge
    {
      unsigned int a, b;
      Edge(unsigned int c, unsigned int d) : a(std::min(c, d)), b(std::max(c, d)) {}
      bool operator<(const Edge &o) const { return a < o.a || (a == o.a && b < o.b); }
    };

    std::map<Edge, unsigned int> newVertexOnEdge;
    std::map<Edge, std::vector<unsigned int>> edgeOppositeVertices;
    std::vector<std::vector<unsigned int>> vertexNeighbors(_vertexPositions.size());
    std::vector<bool> isBoundaryVertex(_vertexPositions.size(), false);

    // II) Topology Analysis
    for (const auto &tri : _triangleIndices)
    {
      for (int i = 0; i < 3; ++i)
      {
        unsigned int v0 = tri[i];
        unsigned int v1 = tri[(i + 1) % 3];
        unsigned int v2 = tri[(i + 2) % 3];
        vertexNeighbors[v0].push_back(v1);
        edgeOppositeVertices[Edge(v0, v1)].push_back(v2);
      }
    }

    // III) Compute New Positions for Even (Original) Vertices
    std::vector<glm::vec3> evenVertexNewPos(_vertexPositions.size());
    for (unsigned int i = 0; i < _vertexPositions.size(); ++i)
    {
      // Remove duplicates to get unique neighbors
      std::sort(vertexNeighbors[i].begin(), vertexNeighbors[i].end());
      vertexNeighbors[i].erase(std::unique(vertexNeighbors[i].begin(), vertexNeighbors[i].end()), vertexNeighbors[i].end());

      // Check for boundary vertices
      std::vector<unsigned int> boundaryNeighbors;
      for (unsigned int neighborIdx : vertexNeighbors[i])
      {
        if (edgeOppositeVertices[Edge(i, neighborIdx)].size() == 1)
        {
          boundaryNeighbors.push_back(neighborIdx);
        }
      }

      if (boundaryNeighbors.size() > 2)
      {
        isBoundaryVertex[i] = false;
      }
      else if (boundaryNeighbors.size() == 2)
      {
        isBoundaryVertex[i] = true;
      }

      // --- Apply Formulas using Alpha/Beta ---
      // --- Apply Formulas using Alpha/Beta from your course ---
      if (isBoundaryVertex[i])
      {
        // Boundary Rule for Even Vertices (this rule is standard)
        glm::vec3 neighborPosSum = _vertexPositions[boundaryNeighbors[0]] + _vertexPositions[boundaryNeighbors[1]];
        evenVertexNewPos[i] = 0.75f * _vertexPositions[i] + 0.125f * neighborPosSum;
      }
      else
      {
        // Interior Rule for Even Vertices
        int n = vertexNeighbors[i].size();
        if (n > 2)
        {
          float alpha, beta;
          if (n == 6)
          {
            // --- ORDINARY VERTEX (n=6) ---
            alpha = 5.0f / 8.0f;
          }
          else
          {
            // --- EXTRAORDINARY VERTEX (n!=6) ---
            float term = 3.0f / 8.0f + 1.0f / 4.0f * cos(2.0f * M_PI / n);
            alpha = 3.0f / 8.0f + term * term;
          }

          beta = (1.0f - alpha) / n;

          glm::vec3 neighborPosSum(0.0f);
          for (unsigned int neighborIdx : vertexNeighbors[i])
          {
            neighborPosSum += _vertexPositions[neighborIdx];
          }
          evenVertexNewPos[i] = alpha * _vertexPositions[i] + beta * neighborPosSum;
        }
        else
        {
          // Isolated vertex or part of a line, keep original position
          evenVertexNewPos[i] = _vertexPositions[i];
        }
      }
    }
    newVertices = evenVertexNewPos;

    // IV) Compute Odd (New) Vertices and Create New Triangles
    for (const auto &tri : _triangleIndices)
    {
      unsigned int v_a = tri[0];
      unsigned int v_b = tri[1];
      unsigned int v_c = tri[2];

      unsigned int oddVertices[3];
      Edge edges[3] = {Edge(v_a, v_b), Edge(v_b, v_c), Edge(v_c, v_a)};

      for (int i = 0; i < 3; ++i)
      {
        if (newVertexOnEdge.find(edges[i]) == newVertexOnEdge.end())
        {
          unsigned int v1 = edges[i].a;
          unsigned int v2 = edges[i].b;

          glm::vec3 newPos;
          const auto &opposites = edgeOppositeVertices[edges[i]];
          if (opposites.size() == 2)
          { // Interior edge
            unsigned int v3 = opposites[0];
            unsigned int v4 = opposites[1];
            newPos = 3.0f / 8.0f * (_vertexPositions[v1] + _vertexPositions[v2]) + 1.0f / 8.0f * (_vertexPositions[v3] + _vertexPositions[v4]);
          }
          else
          { // Boundary edge
            newPos = 0.5f * (_vertexPositions[v1] + _vertexPositions[v2]);
          }

          newVertices.push_back(newPos);
          unsigned int newIndex = newVertices.size() - 1;
          newVertexOnEdge[edges[i]] = newIndex;
          oddVertices[i] = newIndex;
        }
        else
        {
          oddVertices[i] = newVertexOnEdge[edges[i]];
        }
      }

      unsigned int v_ab = oddVertices[0];
      unsigned int v_bc = oddVertices[1];
      unsigned int v_ca = oddVertices[2];

      newTriangles.push_back(glm::uvec3(v_a, v_ab, v_ca));
      newTriangles.push_back(glm::uvec3(v_ab, v_b, v_bc));
      newTriangles.push_back(glm::uvec3(v_ca, v_bc, v_c));
      newTriangles.push_back(glm::uvec3(v_ab, v_bc, v_ca));
    }

    // V) Update Mesh Data
    _vertexPositions = newVertices;
    _triangleIndices = newTriangles;
    recomputePerVertexNormals();
    recomputePerVertexTextureCoordinates();
  }

private:
  std::vector<glm::vec3> _vertexPositions;
  std::vector<glm::vec3> _vertexNormals;
  std::vector<glm::vec2> _vertexTexCoords;
  std::vector<glm::uvec3> _triangleIndices;

  GLuint _vao = 0;
  GLuint _posVbo = 0;
  GLuint _normalVbo = 0;
  GLuint _texCoordVbo = 0;
  GLuint _ibo = 0;
};

// utility: loader
void loadOFF(const std::string &filename, std::shared_ptr<Mesh> meshPtr);

#endif // MESH_H
