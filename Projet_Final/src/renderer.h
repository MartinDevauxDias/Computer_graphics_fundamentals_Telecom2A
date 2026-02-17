#ifndef RENDERER_H
#define RENDERER_H

#include "shader.h"
#include "scene.h"
#include <glm/glm.hpp>

class Renderer {
public:
    bool wireframeMode = false;
    bool raytracingMode = false;

    Renderer(unsigned int screenWidth, unsigned int screenHeight);
    ~Renderer();

    void render(Scene& scene, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);

private:
    Shader rasterShader;
    Shader computeShader;
    Shader screenShader;

    unsigned int screenWidth, screenHeight;
    unsigned int textureOutput;
    unsigned int quadVAO;
    unsigned int triangleSSBO, objectSSBO;

    void initFramebuffers();
    void initShaders();
    void initScreenQuad();
    void initSSBOs();

    void renderRaster(Scene& scene, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);
    void renderRaytraced(Scene& scene, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);
};

#endif // RENDERER_H