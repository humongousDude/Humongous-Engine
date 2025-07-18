[[vk::binding(0, 0)]] RWTexture2D<float4> brdfLUT;
import "../includes/texture_helpers.slang";

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 invocationId : SV_DispatchThreadID) 
{
    uint2 id = invocationId.xy;

    uint2 imageSize;
    brdfLUT.GetDimensions(imageSize.x, imageSize.y);

    if (id.x >= imageSize.x || id.y >= imageSize.y) {
        return;
    }

    float NdotV = (float(id.x) + 0.5f) / float(imageSize.x);
    float roughness = (float(id.y) + 0.5f) / float(imageSize.y);

    float3 V = float3(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);
    float3 N = float3(0.0f, 0.0f, 1.0f);

    float A = 0.0f;
    float B = 0.0f;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        float2 xi = hammersley(i, SAMPLE_COUNT);
        float3 H = sampleGGXDistribution(xi, roughness);
        float3 L = 2.0f * dot(V, H) * H - V;

        float NdotL = max(dot(N, L), 0.0f);
        float NdotH = max(dot(N, H), 0.0f);
        float VdotH = max(dot(V, H), 0.0f);
        float NdotV_clamped = max(0.0f, NdotV);

        if (NdotL > 0.0f) { 
            float G = GeometrySmith(NdotV_clamped, NdotL, roughness);
            float G_Vis = G * (VdotH / (NdotH * NdotV_clamped));
            float Fc = pow(1.0f - VdotH, 5.0f);

            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);

    brdfLUT[id] = float4(A, B, 0.0f, 1.0f);
}
