struct FragmentInput
{
    float2 inUV0        : TEXCOORD0;
    float2 inUV1        : TEXCOORD1;
    float4 inColor0     : COLOR0;
    float3 inWorldPos   : POSITION0; // Use a custom semantic for world position
    float3 inNormal     : NORMAL0;
    float3 inTangent    : TANGENT0;
    float3 inBitTangent : BITANGENT0;
    uint inMaterialIndex : MATERIALID0; // Flat interpolation is implicit with 'uint'
};

// Output structure for multiple render targets (G-Buffer)
struct FragmentOutput
{
    float4 outAlbedo          : SV_Target0; // RGBA: base‐color.rgb, alpha = opacity
    float4 outNormalRoughness : SV_Target1; // RGBA: normal.xyz (0..1), roughness = w
    float4 outMaterialParams  : SV_Target2; // RGBA: emissive.rgb, metallic = a
};

struct MaterialData {
    float4 baseColorFactor;
    float4 emissiveFactor;
    float4 diffuseFactor;
    float4 specularFactor;

    float workflow;
    int baseColorTextureIndex;
    int baseColorTextureSet;
    int physicalDescriptorTextureIndex;

    int physicalDescriptorTextureSet;
    int normalTextureIndex;
    int normalTextureSet;
    int occlusionTextureIndex;

    int occlusionTextureSet;
    int emissiveTextureIndex;
    int emissiveTextureSet;
    float metallicFactor;

    float roughnessFactor;
    float alphaMask;
    float alphaMaskCutoff;
    float emissiveStrength;
};

[[vk::binding(0, 1)]] Sampler2D<float4> textures[]; 
[[vk::binding(1, 1)]] StructuredBuffer<MaterialData> materials; 

// Converts sRGB to linear color
float4 SRGBtoLINEAR(float4 srgbIn)
{
    float3 bLess = step(0.04045f, srgbIn.xyz);
    float3 linOut = lerp(
        srgbIn.xyz / 12.92f,
        pow((srgbIn.xyz + 0.055f) / 1.055f, 2.4f),
        bLess
    );
    return float4(linOut, srgbIn.w);
}

float3 getNormal(MaterialData material, int normalMap, float3 interpolatedNormal, float3 interpolatedTangent, float3 interpolatedBitangent, float2 uv0, float2 uv1)
{
    float3 N = normalize(interpolatedNormal);

    if (normalMap < 0 || material.normalTextureIndex < 0) {
        return N;
    }

    float3 T = normalize(interpolatedTangent);
    float3 B = normalize(interpolatedBitangent);

    // Optional: ensure orthonormal TBN (for high precision)
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt
    B = normalize(cross(N, T)); // Recompute bitangent for accuracy

    float3x3 TBN = float3x3(T, B, N); // T, B, N as column vectors

    float2 uv = (material.normalTextureSet == 0 ? uv0 : uv1);
    // Use the declared defaultSampler
    float3 tangentNormal = textures[normalMap].Sample(uv).xyz * 2.0f - 1.0f;

    // For column-major matrix and vector multiplication: mul(matrix, vector)
    return normalize(mul(TBN, tangentNormal));
}

[shader("fragment")]
FragmentOutput main(FragmentInput input)
{
    FragmentOutput output; 
    MaterialData material = materials[input.inMaterialIndex];

    float4 baseColorLinear = material.baseColorFactor;
    if (material.baseColorTextureIndex >= 0) {
        baseColorLinear *= SRGBtoLINEAR(
            textures[material.baseColorTextureIndex].Sample(material.baseColorTextureSet == 0 ? input.inUV0 : input.inUV1)
        );
    }

    if (material.alphaMask > 0.5f) {
        if (baseColorLinear.a < material.alphaMaskCutoff) {
            discard;
        }
    }

    float roughness = material.roughnessFactor;
    float metallic = material.metallicFactor;
    if (material.physicalDescriptorTextureIndex >= 0) {
        float4 mrSample = textures[material.physicalDescriptorTextureIndex].Sample((material.physicalDescriptorTextureSet == 0 ? input.inUV0 : input.inUV1));
        roughness = clamp(mrSample.g * roughness, 0.04f, 1.0f);
        metallic = clamp(mrSample.b * metallic, 0.0f, 1.0f);
    }

    float3 emissiveCol = material.emissiveFactor.rgb * material.emissiveStrength;
    if (material.emissiveTextureIndex >= 0) {
        emissiveCol *= SRGBtoLINEAR(
            textures[material.emissiveTextureIndex].Sample((material.emissiveTextureSet == 0 ? input.inUV0 : input.inUV1))
        ).rgb;
    }

    float3 N = getNormal(
        material,
        material.normalTextureIndex,
        input.inNormal,
        input.inTangent,
        input.inBitTangent,
        input.inUV0,
        input.inUV1
    );

    float3 n01 = N * 0.5f + 0.5f;

    output.outAlbedo = float4(baseColorLinear.rgb, baseColorLinear.a);
    // output.outAlbedo = float4(input.inUV0.xyxy);
    output.outNormalRoughness = float4(n01, roughness);
    output.outMaterialParams = float4(emissiveCol, metallic);

    return output;
}
