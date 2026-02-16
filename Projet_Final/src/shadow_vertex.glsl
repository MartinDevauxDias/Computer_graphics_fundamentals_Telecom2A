#version 450 core

// Vertex attributes
layout (location = 0) in vec3 aPos;

// Transformation matrices
uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
