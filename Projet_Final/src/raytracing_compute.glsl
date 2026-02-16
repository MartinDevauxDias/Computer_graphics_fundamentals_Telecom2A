#version 450 core

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(rgba32f, binding = 0) uniform image2D imgOutput;

struct Triangle {
    vec4 v0;
    vec4 v1;
    vec4 v2;
    vec4 color;
};

layout(std430, binding = 1) buffer TriangleBuffer {
    Triangle triangles[];
};

struct Object {
    vec4 bmin; // xyz: min, w: reflectivity
    vec4 bmax; // xyz: max, w: triangle start index
    int triangleCount;
    int padding[3];
};

layout(std430, binding = 2) buffer ObjectBuffer {
    Object objects[];
};

uniform int objectCount;
uniform mat4 invView;
uniform mat4 invProjection;
uniform vec3 cameraPos;

struct Ray {
    vec3 origin;
    vec3 direction;
};

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

void main() {
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = imageSize(imgOutput);

    if (texelCoord.x >= size.x || texelCoord.y >= size.y) {
        return;
    }

    // Normalized Device Coordinates [-1, 1]
    vec2 ndc = (vec2(texelCoord) / vec2(size)) * 2.0 - 1.0;
    
    // Ray Generation (Perspective)
    vec4 rayDirClip = vec4(ndc, -1.0, 1.0);
    vec4 rayDirEye = invProjection * rayDirClip;
    rayDirEye = vec4(rayDirEye.xy, -1.0, 0.0);
    vec3 rayDir = normalize((invView * rayDirEye).xyz);
    
    Ray ray;
    ray.origin = cameraPos;
    ray.direction = rayDir;

    vec3 finalColor = vec3(0.0);
    vec3 throughput = vec3(1.0);
    
    // Reflection loop (2 bounces allowed)
    for (int bounce = 0; bounce < 2; bounce++) {
        float closestT = 1e30;
        int hitIdx = -1;

        for (int objIdx = 0; objIdx < objectCount; objIdx++) {
            if (intersectAABB(ray, objects[objIdx].bmin.xyz, objects[objIdx].bmax.xyz)) {
                int start = int(objects[objIdx].bmax.w);
                int count = objects[objIdx].triangleCount;
                for (int i = 0; i < count; i++) {
                    float t;
                    int triIdx = start + i;
                    if (intersectTriangle(ray, triangles[triIdx].v0.xyz, triangles[triIdx].v1.xyz, triangles[triIdx].v2.xyz, t)) {
                        if (t < closestT) {
                            closestT = t;
                            hitIdx = triIdx;
                        }
                    }
                }
            }
        }

        if (hitIdx != -1) {
            vec3 v0 = triangles[hitIdx].v0.xyz;
            vec3 v1 = triangles[hitIdx].v1.xyz;
            vec3 v2 = triangles[hitIdx].v2.xyz;
            vec3 normal = normalize(cross(v1 - v0, v2 - v0));
            
            // Ensure normal faces the ray
            if (dot(normal, ray.direction) > 0.0) normal = -normal;
            
            vec3 hitPoint = ray.origin + ray.direction * closestT;
            float reflectivity = triangles[hitIdx].color.a;

            // Simple diffuse shading
            vec3 lightDir = normalize(vec3(3.0, 5.0, 2.0) - hitPoint);
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 diffuseColor = (diff * triangles[hitIdx].color.rgb + vec3(0.05)) * throughput;
            
            // Apply a portion of the color based on reflectivity
            finalColor += diffuseColor * (1.0 - reflectivity);
            
            if (reflectivity > 0.0) {
                // Update ray for next bounce
                throughput *= triangles[hitIdx].color.rgb * reflectivity;
                ray.origin = hitPoint + normal * 0.001; // Offset to avoid self-intersection
                ray.direction = reflect(ray.direction, normal);
            } else {
                break; // No more bounces if not reflective
            }
        } else {
            // Background color
            finalColor += vec3(0.1, 0.2, 0.3) * throughput;
            break;
        }
    }

    imageStore(imgOutput, texelCoord, vec4(finalColor, 1.0));
}
