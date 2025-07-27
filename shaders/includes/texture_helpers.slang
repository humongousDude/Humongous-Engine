static const float PI = 3.14159265359f;
static const uint SAMPLE_COUNT = 128u;

float vanDerCorput(uint n, uint base) {
    float invBase = 1.0f / float(base);
    float denom = 1.0f;
    float result = 0.0f;
    for (uint i = 0u; i < 32u; ++i) { 
        if (n == 0u) break;
        denom *= float(base);
        result += float(n % base) / denom;
        n /= base;
    }
    return result;
}

float2 hammersley(uint i, uint N) {
    return float2(float(i) / float(N), vanDerCorput(i, 2u));
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0f;
    float nom = NdotV;
    float den = NdotV * (1.0f - k) + k;
    return nom / den;
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

float3 sampleGGXDistribution(float2 xi, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0f * PI * xi.x;
    float cosTheta = sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
    return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float3 cubemapCoords(float2 uv, uint face) {
    uv = uv * 2.0f - 1.0f;
    float3 dir;
    if (face == 0u) dir = float3(1.0f, uv.y, -uv.x);
    else if (face == 1u) dir = float3(-1.0f, uv.y, uv.x);
    else if (face == 2u) dir = float3(uv.x, 1.0f, -uv.y);
    else if (face == 3u) dir = float3(uv.x, -1.0f, uv.y);
    else if (face == 4u) dir = float3(uv.x, uv.y, 1.0f);
    else if (face == 5u) dir = float3(-uv.x, uv.y, -1.0f);
    return dir;
}
