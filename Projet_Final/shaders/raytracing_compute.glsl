#version 450 core

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(rgba32f, binding = 0) uniform image2D imgOutput;
layout(rgba32f, binding = 1) uniform image2D accumulationBuffer;

struct Triangle {
    vec4 v0;
    vec4 v1;
    vec4 v2;
    vec4 color;
    vec4 material; // x: reflectivity, y: roughness, z: ior, w: transparency
};

layout(std430, binding = 1) buffer TriangleBuffer {
    Triangle triangles[];
};

struct Object {
    vec4 bmin; // xyz: min AABB, w: type (0: mesh, 1: sphere)
    vec4 bmax; // xyz: max AABB, w: triangle start index
    int triangleCount;
    float radius;
    float padding[2];
    vec4 emissive;
};

layout(std430, binding = 2) buffer ObjectBuffer {
    Object objects[];
};

uniform int objectCount;
uniform mat4 invView;
uniform mat4 invProjection;
uniform vec3 cameraPos;
uniform int frameCounter;
uniform vec3 skyTop;
uniform vec3 skyBottom;

struct Ray {
    vec3 origin;
    vec3 direction;
};

// Better pseudo-random generator
uint rng_state;
uint hash(uint x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

float random() {
    rng_state = rng_state * 1664525u + 1013904223u;
    return float(rng_state & 0xFFFFFFu) / 16777216.0;
}

// Improved random hemisphere sampling
vec3 randomInHemisphere(vec3 normal) {
    float u1 = random();
    float u2 = random();
    float r = sqrt(u1);
    float theta = 2.0 * 3.14159265 * u2;
    vec3 localDir = vec3(r * cos(theta), r * sin(theta), sqrt(max(0.0, 1.0 - u1)));
    
    vec3 up = abs(normal.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    return normalize(tangent * localDir.x + bitangent * localDir.y + normal * localDir.z);
}

// Ray-AABB intersection
bool intersectAABB(Ray ray, vec3 bmin, vec3 bmax) {
    vec3 invDir = 1.0 / ray.direction;
    vec3 t0 = (bmin - ray.origin) * invDir;
    vec3 t1 = (bmax - ray.origin) * invDir;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float t_start = max(max(tmin.x, tmin.y), tmin.z);
    float t_end = min(min(tmax.x, tmax.y), tmax.z);
    return t_start <= t_end && t_end >= 0.0;
}

// Möller-Trumbore intersection algorithm
bool intersectTriangle(Ray ray, vec3 v0, vec3 v1, vec3 v2, out float t) {
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);
    if (a > -0.00001 && a < 0.00001) return false;
    float f = 1.0 / a;
    vec3 s = ray.origin - v0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return false;
    vec3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0) return false;
    t = f * dot(edge2, q);
    return t > 0.00001;
}

bool intersectSphere(Ray ray, vec3 center, float radius, out float t) {
    vec3 oc = ray.origin - center;
    float a = dot(ray.direction, ray.direction);
    float b = 2.0 * dot(oc, ray.direction);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0) return false;
    
    float t0 = (-b - sqrt(discriminant)) / (2.0 * a);
    float t1 = (-b + sqrt(discriminant)) / (2.0 * a);
    
    if (t0 > 0.00001) {
        t = t0;
        return true;
    }
    if (t1 > 0.00001) {
        t = t1;
        return true;
    }
    return false;
}

void main() {
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(imgOutput);

    if (texelCoord.x >= size.x || texelCoord.y >= size.y) {
        return;
    }

    // Initialize RNG with coordinate and frame-based seed
    rng_state = hash(uint(texelCoord.x + texelCoord.y * size.x + frameCounter * 71939));

    vec3 currentFrameColor = vec3(0.0);
    int samples = 16; // Number of Monte Carlo samples per pixel

    for (int s = 0; s < samples; s++) {
        // Anti-aliasing / Sub-pixel jitter
        vec2 jitter = vec2(random() - 0.5, random() - 0.5);
        vec2 ndc = ((vec2(texelCoord) + jitter) / vec2(size)) * 2.0 - 1.0;
        
        // Ray Generation
        vec4 target = invProjection * vec4(ndc, -1.0, 1.0);
        vec3 rayDir = normalize((invView * vec4(target.xyz, 0.0)).xyz);
        
        Ray ray;
        ray.origin = cameraPos;
        ray.direction = rayDir;

        vec3 sampleColor = vec3(0.0);
        vec3 throughput = vec3(1.0);
        const float EPSILON = 0.005;

        for (int bounce = 0; bounce < 50; bounce++) {
            float closestT = 1e30;
            int hitObjIdx = -1;
            int hitTriIdx = -1;

            for (int objIdx = 0; objIdx < objectCount; objIdx++) {
                if (intersectAABB(ray, objects[objIdx].bmin.xyz, objects[objIdx].bmax.xyz)) {
                    if (objects[objIdx].bmin.w > 0.5) {
                        float t;
                        int start = int(objects[objIdx].bmax.w);
                        vec3 center = triangles[start].v0.xyz;
                        if (intersectSphere(ray, center, objects[objIdx].radius, t)) {
                            if (t < closestT) {
                                closestT = t;
                                hitObjIdx = objIdx;
                                hitTriIdx = -1;
                            }
                        }
                    } else {
                        int start = int(objects[objIdx].bmax.w);
                        int count = objects[objIdx].triangleCount;
                        for (int i = 0; i < count; i++) {
                            float t;
                            if (intersectTriangle(ray, triangles[start + i].v0.xyz, triangles[start + i].v1.xyz, triangles[start + i].v2.xyz, t)) {
                                if (t < closestT) {
                                    closestT = t;
                                    hitObjIdx = objIdx;
                                    hitTriIdx = start + i;
                                }
                            }
                        }
                    }
                }
            }

            // Volumetric Glare/Glow Loop
            for (int i = 0; i < objectCount; i++) {
                if (objects[i].emissive.w > 0.0) {
                    vec3 lightCenter;
                    if (objects[i].bmin.w > 0.5) 
                        lightCenter = triangles[int(objects[i].bmax.w)].v0.xyz;
                    else 
                        lightCenter = (objects[i].bmin.xyz + objects[i].bmax.xyz) * 0.5;
                    
                    float lightRadius = (objects[i].bmin.w > 0.5) ? objects[i].radius : 0.0;
                    float distToLight = length(lightCenter - ray.origin);
                    
                    // Use a small epsilon to ensure glare is added even if the ray hits the light surface
                    if (distToLight - lightRadius <= closestT + 1.0) { 
                        vec3 dirToLight = (lightCenter - ray.origin) / distToLight;
                        float dotL = dot(ray.direction, dirToLight);
                        if (dotL > 0.0) {
                            // Boosted and softened glare for better visibility in reflections
                            float intensity = pow(dotL, 4096.0) * 2.0; // Concentrated core
                            float halo = pow(dotL, 128.0) * 0.07;        // Broader atmospheric glow
                            sampleColor += throughput * objects[i].emissive.xyz * objects[i].emissive.w * (intensity + halo) * 0.6;
                        }
                    }
                }
            }

            if (hitObjIdx != -1) {
                vec4 emission = objects[hitObjIdx].emissive;
                if (emission.w > 0.0) {
                    sampleColor += throughput * emission.xyz * emission.w;
                    break;
                }

                vec3 normal;
                vec4 mat;
                vec3 color;
                int baseTriIdx = int(objects[hitObjIdx].bmax.w);

                if (hitTriIdx == -1) {
                    vec3 center = triangles[baseTriIdx].v0.xyz;
                    vec3 hitPoint = ray.origin + ray.direction * closestT;
                    normal = normalize(hitPoint - center);
                    mat = triangles[baseTriIdx].material;
                    color = triangles[baseTriIdx].color.rgb;
                } else {
                    vec3 v0 = triangles[hitTriIdx].v0.xyz;
                    vec3 v1 = triangles[hitTriIdx].v1.xyz;
                    vec3 v2 = triangles[hitTriIdx].v2.xyz;
                    normal = normalize(cross(v1 - v0, v2 - v0));
                    mat = triangles[hitTriIdx].material;
                    color = triangles[hitTriIdx].color.rgb;
                }
                
                bool outside = dot(normal, ray.direction) < 0.0;
                if (!outside) {
                    // Absorption only for transparent materials
                    if (mat.w > 0.0) {
                        float absorptionStrength = 0.3;
                        vec3 absorption = exp(-absorptionStrength * (vec3(1.0) - color) * closestT);
                        throughput *= absorption;
                    }
                    normal = -normal;
                }
                
                vec3 hitPoint = ray.origin + ray.direction * closestT;
                float r = random();

                if (mat.w > 0.0) {
                    // Dielectric path (Fresnel)
                    float ior = mat.z;
                    float eta = outside ? (1.0 / ior) : ior;
                    float cosTheta = min(dot(-ray.direction, normal), 1.0);
                    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
                    float f0 = (1.0 - ior) / (1.0 + ior); f0 *= f0;
                    float fresnel = f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
                    
                    if (random() < fresnel || eta * sinTheta > 1.0) {
                        ray.direction = reflect(ray.direction, normal);
                        ray.origin = hitPoint + normal * EPSILON;
                    } else {
                        ray.direction = refract(ray.direction, normal, eta);
                        ray.origin = hitPoint - normal * EPSILON;
                    }
                    throughput *= color;
                } else if (r < mat.x) {
                    // Reflection path
                    vec3 reflDir = reflect(ray.direction, normal);
                    if (mat.y > 0.0) reflDir = normalize(mix(reflDir, randomInHemisphere(reflDir), mat.y));
                    ray.direction = reflDir;
                    ray.origin = hitPoint + normal * EPSILON;
                    throughput *= color;
                } else {
                    // Diffuse path
                    ray.direction = randomInHemisphere(normal);
                    ray.origin = hitPoint + normal * EPSILON;
                    throughput *= color;
                }
            } else {
                float t = 0.5 * (ray.direction.y + 1.0);
                sampleColor += throughput * mix(skyBottom, skyTop, t);
                break;
            }

            if (length(throughput) < 0.01) break;
        }
        currentFrameColor += sampleColor;
    }

    currentFrameColor /= float(samples);

    vec3 finalColor = currentFrameColor;
    if (frameCounter > 1) {
        vec3 prevColor = imageLoad(accumulationBuffer, texelCoord).rgb;
        finalColor = mix(prevColor, currentFrameColor, 1.0 / float(frameCounter));
    }

    imageStore(accumulationBuffer, texelCoord, vec4(finalColor, 1.0));
    imageStore(imgOutput, texelCoord, vec4(finalColor, 1.0));
}
