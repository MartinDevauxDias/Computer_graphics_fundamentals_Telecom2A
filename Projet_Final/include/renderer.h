#ifndef RENDERER_H
#define RENDERER_H

#include "shader.h"
#include "gputypes.h"
#include <vector>
#include <glm/glm.hpp>

class Scene;

class Renderer {
public:
    Renderer(unsigned int w, unsigned int h);
    ~Renderer();

    double render(Scene& scene, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, bool raytracingMode, bool wireframeMode);
    
    void resize(unsigned int w, unsigned int h);
    void renderRaster(Scene& scene, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, bool wireframeMode);
    double renderRaytraced(Scene& scene, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);

    void initFramebuffers();
    void initScreenQuad();
    void initSSBOs();

    unsigned int screenWidth, screenHeight;
    unsigned int frameCounter = 1;
    glm::vec3 lastCameraPos = glm::vec3(0.0f);
    glm::mat4 lastView = glm::mat4(1.0f);

private:
    Shader rasterShader;
    Shader computeShader;
    Shader screenShader;

    unsigned int textureOutput;
    unsigned int accumulationTexture;
    unsigned int quadVAO;
    unsigned int triangleSSBO;
    unsigned int objectSSBO;
};

#endif // RENDERER_H
