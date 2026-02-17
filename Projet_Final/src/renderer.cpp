#include "renderer.h"
#include "gputypes.h"
#include <glad/glad.h>
#include <omp.h>
#include <vector>
#include <GLFW/glfw3.h>

Renderer::Renderer(unsigned int w, unsigned int h)
    : rasterShader("src/vertex_shader.glsl", "src/fragment_shader.glsl"),
      computeShader("src/raytracing_compute.glsl"),
      screenShader("src/screen_vertex.glsl", "src/screen_fragment.glsl"),
      screenWidth(w), screenHeight(h)
{
    initFramebuffers();
    initScreenQuad();
    initSSBOs();
    glEnable(GL_DEPTH_TEST);
}

Renderer::~Renderer() {
    glDeleteTextures(1, &textureOutput);
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

void Renderer::renderRaster(Scene& scene, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, bool wireframeMode) {
    glEnable(GL_DEPTH_TEST);
    
    if (wireframeMode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    rasterShader.use();
    rasterShader.set("lightPos", glm::vec3(3.0f, 5.0f, 2.0f));
    rasterShader.set("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
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
        Object* obj = scene.objects[i];
        glm::vec4 color;
        float reflect = 0.0f;

        if (obj->fixedObject) {
            color = glm::vec4(0.3f, 0.5f, 0.3f, 0.0f);
            reflect = 0.6f;
        } else if (obj->collisionRadius > 0.0f) {
            color = glm::vec4(1.0f, 0.2f, 0.2f, 0.0f);
            reflect = 0.0f;
        } else {
            float r = 0.4f + 0.4f * sin(i * 0.5f);
            float g = 0.4f + 0.4f * cos(i * 0.3f);
            float b = 0.6f + 0.2f * sin(i * 0.8f);
            color = glm::vec4(r, g, b, 0.0f);
            reflect = 0.2f;
        }

        glm::mat4 model = obj->getModelMatrix();
        size_t startIdx = offsets[i];
        size_t numTris = obj->mesh->indices.size() / 3;
        glm::vec3 bmin(1e30f), bmax(-1e30f);

        for (size_t j = 0; j < numTris; ++j) {
            GPUTriangle& tri = gpuTriangles[startIdx + j];
            tri.v0 = model * glm::vec4(obj->mesh->vertices[obj->mesh->indices[j*3]].position, 1.0f);
            tri.v1 = model * glm::vec4(obj->mesh->vertices[obj->mesh->indices[j*3+1]].position, 1.0f);
            tri.v2 = model * glm::vec4(obj->mesh->vertices[obj->mesh->indices[j*3+2]].position, 1.0f);
            tri.color = glm::vec4(glm::vec3(color), reflect);
            bmin = glm::min(bmin, glm::vec3(tri.v0)); bmin = glm::min(bmin, glm::vec3(tri.v1)); bmin = glm::min(bmin, glm::vec3(tri.v2));
            bmax = glm::max(bmax, glm::vec3(tri.v0)); bmax = glm::max(bmax, glm::vec3(tri.v1)); bmax = glm::max(bmax, glm::vec3(tri.v2));
        }
        gpuObjects[i] = { glm::vec4(bmin, reflect), glm::vec4(bmax, float(startIdx)), int(numTris) };
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
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangleSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, objectSSBO);
    glDispatchCompute(screenWidth / 16, screenHeight / 16, 1);
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