// Material class for managing surface properties and textures
#ifndef MATERIAL_H
#define MATERIAL_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class Material {
public:
    // Surface properties
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;

    // Texture IDs
    unsigned int diffuseTexture;
    unsigned int specularTexture;

    // Constructors
    Material();
    Material(const std::string& diffusePath);

    // Bind material and textures for rendering
    void use(class Shader& shader);

    // Release texture resources
    void cleanup();

private:
    // Load texture from file and return OpenGL texture ID
    unsigned int loadTexture(const std::string& path);
};

#endif