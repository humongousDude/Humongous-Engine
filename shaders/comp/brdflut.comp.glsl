#version 460

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Output BRDF 2D LUT (storage image)
layout(binding = 0) uniform writeonly image2D brdfLUT;

// Constants for computation
const float PI = 3.14159265359;
const uint SAMPLE_COUNT = 1024u; // Number of samples for the integral

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

// Geometry Schlick-GGX
float GeometrySchlickGGX(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;
    float nom = NdotV;
    float den = NdotV * (1.0 - k) + k;
    return nom / den;
}

// Geometry Smith (combines G_in and G_out)
float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Helper function to sample the GGX NDF and return the half vector H
vec3 sampleGGXDistribution(vec2 xi, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

void main() {
    // Global invocation ID corresponds to output texel (x, y)
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageSize = imageSize(brdfLUT);
    if (id.x >= imageSize.x || id.y >= imageSize.y) {
        return; // Handle potential out-of-bounds invocations
    }

    // Map texel coordinates to NdotV and roughness
    float NdotV = (float(id.x) + 0.5) / float(imageSize.x);
    float roughness = (float(id.y) + 0.5) / float(imageSize.y);

    // Create view vector V (pointing up) and normal N (pointing up)
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV); // View vector in tangent space
    vec3 N = vec3(0.0, 0.0, 1.0); // Normal in tangent space

    float A = 0.0; // Stores the (1 - F) * G_Vis contribution
    float B = 0.0; // Stores the F * G_Vis contribution

    // Integrate the BRDF using importance sampling
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 xi = hammersley(i, SAMPLE_COUNT);

        vec3 H = sampleGGXDistribution(xi, roughness); // Sample the half vector H

        vec3 L = 2.0 * dot(V, H) * H - V; // Light direction

        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);
        float NdotV_clamped = max(0.0, NdotV); // Clamp NdotV for GGX

        if (NdotL > 0.0) {
            float G = GeometrySmith(NdotV_clamped, NdotL, roughness);
            float G_Vis = G * (VdotH / (NdotH * NdotV_clamped)); // Geometric Shadowing and Visibility term
            float Fc = pow(1.0 - VdotH, 5.0); // Fresnel term (Schlick's approximation with F0 = 1 for white)

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);

    imageStore(brdfLUT, id, vec4(A, B, 0.0, 1.0));
}
