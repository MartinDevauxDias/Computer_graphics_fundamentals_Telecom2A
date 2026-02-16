#version 450 core

// Vertex attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aColor;

// Outputs to fragment shader
out vec3 FragPos;
out vec3 Normal;
out vec3 vertexColor;
out vec2 texCoord;
out vec4 FragPosLightSpace;

// Transformation matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

void main()
{
    // Position in world space
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // Normal in world space (using normal matrix)
    Normal = mat3(transpose(inverse(model))) * aNormal;
    
    // Position in light space for shadow mapping
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    
    // Position in clip space
    gl_Position = projection * view * vec4(FragPos, 1.0);
    
    vertexColor = aColor;
    texCoord = aTexCoord;
}