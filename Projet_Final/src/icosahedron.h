#ifndef ICOSAHEDRON_H
#define ICOSAHEDRON_H

#include "mesh.h"

class Icosahedron : public Mesh {
public:
    // Create a regular icosahedron with given radius
    Icosahedron(float radius = 1.0f);
    
    // Create an Icosphere by subdividing an icosahedron multiple times
    // This maintains the "regularity" of the triangle distribution
    static Mesh* createIcosphere(float radius, unsigned int subdivisions);

private:
    void buildIcosahedron(float radius);
};

#endif
