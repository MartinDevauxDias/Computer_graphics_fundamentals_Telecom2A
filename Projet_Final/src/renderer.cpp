#include "renderer.h"
#include "scene.h"
#include "object.h"
#include <glad/glad.h>
#include <omp.h>
#include <vector>
#include <GLFW/glfw3.h>

Renderer::Renderer(unsigned int w, unsigned int h)
    : rasterShader("shaders/vertex_shader.glsl", "shaders/fragment_shader.glsl"),
      computeShader("shaders/raytracing_compute.glsl"),
      screenShader("shaders/screen_vertex.glsl", "shaders/screen_fragment.glsl"),
      screenWidth(w), screenHeight(h)
{
    initFramebuffers();
    initScreenQuad();
    initSSBOs();
    glEnable(GL_DEPTH_TEST);
}

Renderer::~Renderer() {
    glDeleteTextures(1, &textureOutput);
    glDeleteTextures(1, &accumulationTexture);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &triangleSSBO);
    glDeleteBuffers(1, &objectSSBO);
}

double Renderer::render(Scene& scene, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, bool raytracingMode, bool wireframeMode) {
    glViewport(0, 0, screenWidth, screenHeight);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (raytracingMode) {
        return renderRaytraced(scene, view, projection, cameraPos);
    } else {
        renderRaster(scene, view, projection, cameraPos, wireframeMode);
        return 0.0;
    }
}

void Renderer::resize(unsigned int w, unsigned int h) {
    if (w == screenWidth && h == screenHeight) return;
    screenWidth = w;
    screenHeight = h;

    // Delete old textures before recreating them
    glDeleteTextures(1, &textureOutput);
    glDeleteTextures(1, &accumulationTexture);

    initFramebuffers();
    frameCounter = 1; // Reset accumulation
}

void Renderer::renderRaster(Scene& scene, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, bool wireframeMode) {
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, screenWidth, screenHeight);
    
    if (wireframeMode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    rasterShader.use();
    
    // Find the first emissive object to use as the light source for rasterization
    glm::vec3 lPos(30.0f, 50.0f, 20.0f);
    glm::vec3 lCol(1.0f, 0.9f, 0.7f);
    for (auto* obj : scene.objects) {
        if (obj->material && glm::length(obj->material->emissive) > 0.1f) {
            lPos = obj->position;
            lCol = obj->material->emissive * obj->material->emissiveStrength;
            break;
        }
    }
    rasterShader.set("lightPos", lPos);
    rasterShader.set("lightColor", lCol);
    
    rasterShader.set("viewPos", cameraPos);
    rasterShader.set("view", view);
    rasterShader.set("projection", projection);

    for (auto* obj : scene.objects) {
        if (obj->fixedObject) {
            rasterShader.set("objectColor", 0.3f, 0.5f, 0.3f);
        } else if (obj->collisionRadius > 0.0f) {
            rasterShader.set("objectColor", 1.0f, 0.2f, 0.2f);
        } else {
            rasterShader.set("objectColor", 0.4f, 0.4f, 0.8f);
        }
        obj->draw(rasterShader);
    }
}

double Renderer::renderRaytraced(Scene& scene, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) {
    glDisable(GL_DEPTH_TEST);
    double prepStart = glfwGetTime();

    // Determine if scene is static (no non-fixed objects)
    bool isStatic = true;
    for (auto* obj : scene.objects) {
        if (!obj->fixedObject) {
            isStatic = false;
            break;
        }
    }

    // Reset frame counter if camera moves or scene is dynamic
    if (cameraPos != lastCameraPos || view != lastView || !isStatic) {
        frameCounter = 1;
        lastCameraPos = cameraPos;
        lastView = view;
    } else {
        frameCounter++;
    }

    // 1. Prepare data for GPU
    std::vector<size_t> offsets(scene.objects.size() + 1);
    offsets[0] = 0;
    for(size_t i = 0; i < scene.objects.size(); ++i) {
        offsets[i+1] = offsets[i] + scene.objects[i]->mesh->indices.size() / 3;
    }

    size_t totalTris = offsets.back();
    if (totalTris == 0) return 0.0;

    std::vector<GPUTriangle> gpuTriangles(totalTris);
    std::vector<GPUObject> gpuObjects(scene.objects.size());

    #pragma omp parallel for
    for (int i = 0; i < (int)scene.objects.size(); ++i) {
        scene.objects[i]->toGPU(gpuObjects[i], gpuTriangles, offsets[i]);
    }

    double prepTime = glfwGetTime() - prepStart;

    // 2. Update SSBOs
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangleSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gpuTriangles.size() * sizeof(GPUTriangle), gpuTriangles.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, objectSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gpuObjects.size() * sizeof(GPUObject), gpuObjects.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // 3. Dispatch Compute Shader
    computeShader.use();
    computeShader.set("objectCount", (int)gpuObjects.size());
    computeShader.set("invView", glm::inverse(view));
    computeShader.set("invProjection", glm::inverse(projection));
    computeShader.set("cameraPos", cameraPos);
    computeShader.set("frameCounter", (int)frameCounter);
    computeShader.set("skyTop", scene.skyTop);
    computeShader.set("skyBottom", scene.skyBottom);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangleSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, objectSSBO);
    
    glBindImageTexture(0, textureOutput, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(1, accumulationTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    glDispatchCompute((screenWidth + 15) / 16, (screenHeight + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 4. Render result to screen
    screenShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureOutput);
    screenShader.set("screenTexture", 0);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    return prepTime;
}

// --- Initialization Functions ---
void Renderer::initFramebuffers() {
    glGenTextures(1, &textureOutput);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureOutput);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, textureOutput, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    glGenTextures(1, &accumulationTexture);
    glBindTexture(GL_TEXTURE_2D, accumulationTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(1, accumulationTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
}

void Renderer::initScreenQuad() {
    float quadVertices[] = { -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f };
    unsigned int quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
}

void Renderer::initSSBOs() {
    glGenBuffers(1, &triangleSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangleSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &objectSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, objectSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}