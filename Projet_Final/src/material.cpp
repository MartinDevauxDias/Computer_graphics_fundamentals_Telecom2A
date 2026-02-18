#include "object.h"
#include "shader.h"
#include "stb_image.h"
#include <iostream>

Material::Material() 
    : ambient(0.1f), diffuse(0.7f), specular(0.2f), shininess(32.0f), 
      reflectivity(0.0f), roughness(0.0f), transparency(0.0f), ior(1.5f),
      emissive(0.0f), emissiveStrength(0.0f), diffuseTexture(0), specularTexture(0) {}

Material::~Material() {}

Material::Material(const std::string& diffusePath) 
    : ambient(0.1f), diffuse(1.0f), specular(0.5f), shininess(32.0f),
      reflectivity(0.0f), roughness(0.0f), transparency(0.0f), ior(1.5f),
      emissive(0.0f), emissiveStrength(0.0f), specularTexture(0) {
    diffuseTexture = loadTexture(diffusePath);
}

// Bind material properties and textures for rendering
void Material::use(Shader &shader)
{
    shader.use();

    // Set material properties
    shader.set("material.ambient", ambient);
    shader.set("material.diffuse", diffuse);
    shader.set("material.specular", specular);
    shader.set("material.shininess", shininess);

    // Bind diffuse texture to texture unit 0
    if (diffuseTexture != 0)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseTexture);
        shader.set("material.diffuseMap", 0);
    }

    // Bind specular texture to texture unit 1
    if (specularTexture != 0)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, specularTexture);
        shader.set("material.specularMap", 1);
    }
}

// Release texture resources
void Material::cleanup()
{
    glDeleteTextures(1, &diffuseTexture);
    glDeleteTextures(1, &specularTexture);
}

// Load texture from file and return OpenGL texture ID
unsigned int Material::loadTexture(const std::string &path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    // Load image data
    int width, height, nrChannels;
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);

    if (data)
    {
        // Determine format based on number of channels
        GLenum format;
        if (nrChannels == 1)
            format = GL_RED;
        else if (nrChannels == 3)
            format = GL_RGB;
        else if (nrChannels == 4)
            format = GL_RGBA;

        // Upload texture data to GPU
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Configure texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Failed to load texture at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
