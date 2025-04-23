#version 460

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Input environment cubemap (sampler)
layout(set = 0, binding = 0) uniform samplerCube environmentMap;

// Output prefiltered cubemap (storage image array for mip levels)
// Note: In Vulkan, you'll bind views to specific mip levels here.
// The binding point might represent the base level, and you use a view for the specific mip.
// This GLSL uses an array syntax as a conceptual representation.
// A more typical Vulkan GLSL setup might bind a single imageCube and use a uniform for the mip level.
layout(set = 1, binding = 0, rgba16f) uniform writeonly imageCube prefilteredMap;

layout(push_constant) uniform Constants {
    float roughness; // Roughness for the current mip level
    uint mipLevel; // Current mipmap level being generated
} constants;

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

// Distribution Specular - GGX Normal Distribution Function
float DistributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / denom;
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
    // Global invocation ID corresponds to output texel (x, y) and face (z) for the current mip level
    ivec3 id = ivec3(gl_GlobalInvocationID);
    ivec2 imageSize = imageSize(prefilteredMap);
    if (id.x >= imageSize.x || id.y >= imageSize.y) {
        return; // Handle potential out-of-bounds invocations
    }

    vec2 uv = (vec2(id.xy) + 0.5) / vec2(imageSize);
    int face = id.z;

    vec3 N = normalize(cubemapCoords(uv, face)); // Get world space normal
    vec3 R = N; // Assume view direction V is equal to normal N for simplicity in this precomputation

    float totalWeight = 0.0;
    vec3 prefilteredColor = vec3(0.0);

    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    // Generate SAMPLE_COUNT samples over the hemisphere, biased towards R based on roughness
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 xi = hammersley(i, SAMPLE_COUNT);

        // Sample the GGX NDF
        float phi = 2.0 * PI * xi.x;
        float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (constants.roughness * constants.roughness - 1.0) * xi.y));
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta); // Halfway vector in tangent space

        // Tangent space to world space
        H = right * H.x + up * H.y + N * H.z;

        vec3 L = 2.0 * dot(R, H) * H - R; // Light direction

        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(R, H), 0.0); // R is used as V here

        if (NdotL > 0.0) {
            // This is a simplified weighting. A more accurate approach weights by the BRDF's specular part.
            // float D = DistributionGGX(NdotH, constants.roughness);
            // float G = GeometrySmith(NdotV, NdotL, constants.roughness); // NdotV = NdotR = 1.0 here
            // float pdf = (D * NdotH) / (4.0 * VdotH);
            // float solidAngle = 1.0 / (float(SAMPLE_COUNT) * pdf);
            // prefilteredColor += texture(environmentMap, L).rgb * NdotL * solidAngle;

            // Simple weighting by NdotL
            prefilteredColor += texture(environmentMap, L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor /= totalWeight;

    // imageStore(prefilteredMap[constants.mipLevel], ivec3(id.xy, face), vec4(prefilteredColor, 1.0));
    // When using a single imageCube binding and a mip level uniform:
    imageStore(prefilteredMap, ivec3(id.xy, face), vec4(prefilteredColor, 1.0));
}
