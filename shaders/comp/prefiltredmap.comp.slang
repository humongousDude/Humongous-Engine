[[vk::binding(0, 0)]] SamplerCube environmentMap; 
[[vk::binding(0, 1)]] RWTexture2DArray prefilteredMap; 

[[vk::push_constant]]
cbuffer Constants {
    float roughness; 
    uint mipLevel;   
} constants;

import "../includes/texture_helpers.slang";

[shader("compute")]
[numthreads(16, 16, 1)] 
void main(uint3 gl_GlobalInvocationID : SV_DispatchThreadID)
{
    uint3 id = gl_GlobalInvocationID; 

    uint mips, width, height, levels;
    mips = 10;
    environmentMap.GetDimensions(constants.mipLevel, width, height, levels);

    if (id.x >= width || id.y >= height) {
        return;
    }

    float2 uv = (float2(id.xy) + 0.5f) / float2(width, height);
    uint face = id.z;

    float3 N = normalize(cubemapCoords(uv, face));
    float3 R = N;

    float totalWeight = 0.0f;
    float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);

    float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        float2 xi = hammersley(i, SAMPLE_COUNT);

        float phi = 2.0f * PI * xi.x;
        float cosTheta = sqrt((1.0f - xi.y) / (1.0f + (constants.roughness * constants.roughness - 1.0f) * xi.y));
        float sinTheta = sqrt(1.0f - cosTheta * cosTheta); 
        float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta); 

        H = right * H.x + up * H.y + N * H.z;

        float3 L = 2.0f * dot(R, H) * H - R; 

        float NdotL = max(dot(N, L), 0.0f); 
        float NdotH = max(dot(N, H), 0.0f); 
        float VdotH = max(dot(R, H), 0.0f); 

        float sampleLod = constants.roughness * float(mips - 1);
        float3 envColor = environmentMap.SampleLevel(L, sampleLod).rgb;

        if (NdotL > 0.0f) { 
            prefilteredColor += envColor * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor /= totalWeight;

    prefilteredMap[uint3(id.x, id.y, id.z)] = float4(prefilteredColor, 1.0f);
}
