[[vk::binding(0, 0)]] cbuffer UBO
{
    float4x4 projection;
    float4x4 invProjection;
    float4x4 view;
    float4x4 invView;
    float4x4 projectionView;
    float3 cameraPos; 
} ubo;

[[vk::binding(0, 1)]] cbuffer UBOParams 
{
    float3 camPos;
    float _padding0;
    float4 lightDir;
    float exposure;
    float gamma;
    float radiance;
    float prefilteredCubeMipLevels;
    float scaleIBLAmbient;
    int debugViewInputs;
    int debugViewEquation;
} uboParams;

[[vk::binding(1, 2)]] SamplerCube samplerIrradiance;
[[vk::binding(2, 2)]] SamplerCube prefilteredMap;
[[vk::binding(3, 2)]] Sampler2D samplerBRDFLUT;

[[vk::binding(0, 3)]] Sampler2D gAlbedo;
[[vk::binding(1, 3)]] Sampler2D gNormalRough;
[[vk::binding(2, 3)]] Sampler2D gMatParams;
[[vk::binding(3, 3)]] Sampler2D gDepth;
[[vk::binding(4, 3)]] RWTexture2D<float4> drawImage;

static const float PI = 3.14159265359f;

float3 ReconstructWorldPos(float2 uv, float depth, uint2 imageDims) 
{
    float4 clipPosH;
    clipPosH.xy = uv * 2.0f - 1.0f;
    clipPosH.z = depth;
    clipPosH.w = 1.0f;

    float4 viewPosH = mul(ubo.invProjection, clipPosH);
    float3 viewPos = viewPosH.xyz / viewPosH.w;       

    float4 worldPosH = mul(ubo.invView, float4(viewPos, 1.0f));
    return worldPosH.xyz;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float denom = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 gl_GlobalInvocationID : SV_DispatchThreadID) 
{
    uint2 pixelCoords = gl_GlobalInvocationID.xy;
    uint2 imageDims;
    drawImage.GetDimensions(imageDims.x, imageDims.y);

    float2 uv = (float2(pixelCoords) + 0.5f) / float2(imageDims);


    float4 albSample = float4(0);
    float4 nrSample  = float4(0);
    float4 mpSample  = float4(0);
    float depth      = 0;

    float3 albedo = albSample.rgb;
    float4 outColor;

    if (uboParams.debugViewInputs > 0) { 
        int index = uboParams.debugViewInputs;
        switch (index) {
            case 1:
                albSample = gAlbedo.Sample(uv);
                outColor.rgba = albSample;
                break;
            case 2:
                nrSample = gNormalRough.Sample(uv);
                outColor.rgba = nrSample;
                break;
            case 3:
                mpSample = gMatParams.Sample(uv);
                outColor.rgba = mpSample;
                break;
            case 4:
                depth = gDepth.Sample(uv).r;
                outColor.rgba = float4(depth, depth, depth, 1.0f);
                break;
            case 5:
                float3 p = ReconstructWorldPos(uv, depth, imageDims);
                outColor.rgba = float4(p, 1.0f);
                break;
        }
        drawImage[pixelCoords] = outColor;
        return;
    }
    else{
         albSample = gAlbedo.Sample(uv);
         nrSample = gNormalRough.Sample(uv);
         mpSample = gMatParams.Sample(uv);
         depth = gDepth.Sample(uv).r;

         albedo = albSample.rgb;
         outColor;
    }

    float alpha = albSample.a;
    float3 N = normalize(nrSample.xyz * 2.0f - 1.0f);
    float roughness = nrSample.w;

    float3 emissive = mpSample.rgb;
    float metallic = mpSample.a;
    float3 worldPos = ReconstructWorldPos(uv, depth, imageDims);
    float3 V = normalize(uboParams.camPos - worldPos);
    float3 L = normalize(-uboParams.lightDir.xyz);
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0f);
    float NdotV = max(dot(N, V), 0.0f);
    float NdotH = max(dot(N, H), 0.0f);
    float VdotH = max(dot(V, H), 0.0f);

    float ambient = 0.01f;

    float3 fragcolor = albedo * ambient;

    float specularStrength = alpha;

    float3 singularLightColor = float3(1.0f, 1.0f, 1.0f);
    float singularLightIntensity = 1.0f;
    float3 diff = singularLightColor * albedo.rgb * NdotL * singularLightIntensity;

    float3 R = reflect(-L, N);
    float RdotV = max(0.0f, dot(R, V));
    float shininess = 16.0f;
    float3 spec = singularLightColor * specularStrength * pow(RdotV, shininess) * singularLightIntensity;

    fragcolor += diff + spec;

    float3 F0_PBR = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float3 irradiance = samplerIrradiance.Sample(N).rgb;
    float3 diffuseIBL_base = irradiance * albedo;

    float3 R_ibl = reflect(-V, N);
    float3 prefiltered_ibl = prefilteredMap.SampleLevel(R_ibl, roughness * uboParams.prefilteredCubeMipLevels).rgb;

    float2 brdf_lut_sample = samplerBRDFLUT.Sample(float2(NdotV, roughness)).rg;

    float3 specular_ibl_component = prefiltered_ibl * (F0_PBR * brdf_lut_sample.x + brdf_lut_sample.y);

    float3 kS_ibl_fresnel = F0_PBR;
    float3 kD_ibl = (float3(1.0f, 1.0f, 1.0f) - kS_ibl_fresnel) * (1.0f - metallic);
    float3 diffuse_ibl_component = kD_ibl * diffuseIBL_base;

    float3 ambient_PBR = (diffuse_ibl_component + specular_ibl_component) * uboParams.scaleIBLAmbient;

    fragcolor += ambient_PBR;
    fragcolor += emissive;

    float3 color = float3(1.0f, 1.0f, 1.0f) - exp(-fragcolor * uboParams.exposure);
    color = pow(color, float3(1.0f / uboParams.gamma));
    outColor = float4(color, 1.0f);

    drawImage[pixelCoords] = outColor;
}
