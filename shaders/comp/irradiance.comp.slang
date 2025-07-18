[[vk::binding(0, 0)]] SamplerCube environmentMap;
[[vk::binding(0, 1)]] RWTexture2DArray irradianceMap;

import "../includes/texture_helpers.slang";

[[shader("compute")]]
[[numthreads(16, 16, 1)]] 
void main(uint3 gl_GlobalInvocationID : SV_DispatchThreadID) 
{
    uint3 id = gl_GlobalInvocationID;

    uint width, height, depth;
    irradianceMap.GetDimensions(width, height, depth);

    if (id.x >= width || id.y >= height || id.z >= depth) {
        return;
    }

    float2 uv = (float2(id.xy) + 0.5f) / float2(width, height);
    uint face = id.z;

    float3 N = normalize(cubemapCoords(uv, face));

    float3 irradiance = float3(0.0f, 0.0f, 0.0f);
    float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        float2 xi = hammersley(i, SAMPLE_COUNT);

        float phi = 2.0f * PI * xi.x;
        float cosTheta = sqrt(1.0f - xi.y);
        float sinTheta = sqrt(xi.y);

        float3 sampleVector = float3(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));

        float3 sampleDir = right * sampleVector.x + up * sampleVector.y + N * sampleVector.z;

        irradiance += environmentMap.Sample(sampleDir).rgb;
    }

    irradiance /= float(SAMPLE_COUNT);

    irradianceMap[uint3(id.x, id.y, id.z)] = float4(irradiance, 1.0f);
}
