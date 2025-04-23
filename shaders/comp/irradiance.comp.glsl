#version 460

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Input environment cubemap (sampler)
layout(set = 0, binding = 0) uniform samplerCube environmentMap;

// Output irradiance cubemap (storage image)
layout(set = 1, binding = 0, rgba16f) uniform writeonly imageCube irradianceMap;

// Constants for convolution
const float PI = 3.14159265359;
const uint SAMPLE_COUNT = 1024u; // Number of samples for convolution

// Van der Corput sequence for Hammersley
float vanDerCorput(uint n, uint base) {
    float invBase = 1.0 / float(base);
    float denom = 1.0;
    float result = 0.0;
    for (uint i = 0u; i < 32u; ++i) {
        if (n == 0u) break;
        denom *= float(base);
        result += float(n % base) / denom;
        n /= base;
    }
    return result;
}

// Hammersley sequence for 2D points
vec2 hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), vanDerCorput(i, 2u));
}

// Helper function to get cubemap direction from UV and face
vec3 cubemapCoords(vec2 uv, int face) {
    uv = uv * 2.0 - 1.0;
    vec3 dir;
    if (face == 0) dir = vec3(1.0, uv.y, -uv.x); // +X
    if (face == 1) dir = vec3(-1.0, uv.y, uv.x); // -X
    if (face == 2) dir = vec3(uv.x, 1.0, -uv.y); // +Y
    if (face == 3) dir = vec3(uv.x, -1.0, uv.y); // -Y
    if (face == 4) dir = vec3(uv.x, uv.y, 1.0); // +Z
    if (face == 5) dir = vec3(-uv.x, uv.y, -1.0); // -Z
    return dir;
}

void main() {
    // Global invocation ID corresponds to output texel (x, y) and face (z)
    ivec3 id = ivec3(gl_GlobalInvocationID);
    ivec2 imageSize = imageSize(irradianceMap);
    if (id.x >= imageSize.x || id.y >= imageSize.y) {
        return; // Handle potential out-of-bounds invocations
    }

    vec2 uv = (vec2(id.xy) + 0.5) / vec2(imageSize);
    int face = id.z;

    vec3 N = normalize(cubemapCoords(uv, face)); // Get world space normal from cubemap face and UV

    vec3 irradiance = vec3(0.0);
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    // Generate SAMPLE_COUNT samples over the hemisphere using Hammersley sequence
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 xi = hammersley(i, SAMPLE_COUNT);

        // Hemisphere sample (cosine weighted)
        float phi = 2.0 * PI * xi.x;
        float cosTheta = sqrt(1.0 - xi.y);
        float sinTheta = sqrt(xi.y);

        // Convert to Cartesian coordinates in tangent space
        vec3 sampleVector = vec3(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));

        // Tangent space to world space
        vec3 sampleDir = right * sampleVector.x + up * sampleVector.y + N * sampleVector.z;

        irradiance += texture(environmentMap, sampleDir).rgb;
    }

    irradiance /= float(SAMPLE_COUNT);

    imageStore(irradianceMap, id, vec4(irradiance, 1.0));
}
