// Shader class for loading, compiling, and managing OpenGL shaders
#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <unordered_map>

class Shader
{
public:
    unsigned int ID;   // Shader program ID

    // Constructor: load and compile shaders from file paths
    Shader(const char *vertexPath, const char *fragmentPath);
    Shader(const char *computePath); // Compute shader constructor
    ~Shader();

    // Activate the shader program
    void use() const;

    // Deactivate the current shader program
    static void stop();

    // Uniform setters for different types
    void set(const std::string &name, bool value) const;
    void set(const std::string &name, int value) const;
    void set(const std::string &name, float value) const;
    void set(const std::string &name, const glm::vec2 &value) const;
    void set(const std::string &name, const glm::vec3 &value) const;
    void set(const std::string &name, const glm::vec4 &value) const;
    void set(const std::string &name, const glm::mat3 &value) const;
    void set(const std::string &name, const glm::mat4 &value) const;
    void set(const std::string &name, float x, float y, float z) const;

private:
    // Caches uniform locations to improve performance
    mutable std::unordered_map<std::string, int> uniformLocationCache;
    int getUniformLocation(const std::string &name) const;
};

#endif