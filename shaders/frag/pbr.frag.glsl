#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inUV0;
layout(location = 1) in vec2 inUV1;
layout(location = 2) in vec4 inColor0;
layout(location = 3) in vec3 inWorldPos;
layout(location = 4) in vec3 inNormal;
layout(location = 5) in vec3 inTangent;
layout(location = 6) in vec3 inBitTangent;
layout(location = 7) flat in uint inMaterialIndex;

layout(location = 0) out vec4 outAlbedo; // RGBA: base‐color.rgb, alpha = opacity
layout(location = 1) out vec4 outNormalRoughness; // RGBA: normal.xyz (0..1), roughness = w
layout(location = 2) out vec4 outMaterialParams; // RGBA: emissive.rgb, metallic = a
layout(location = 3) out vec4 outPosition;

struct MaterialData {
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 diffuseFactor;
    vec4 specularFactor;

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

layout(set = 1, binding = 0) uniform sampler2D textures[];

layout(std430, set = 1, binding = 1) readonly buffer SSBO
{
    MaterialData materials[];
};

// Converts sRGB to linear color
vec4 SRGBtoLINEAR(vec4 srgbIn)
{
    vec3 bLess = step(vec3(0.04045), srgbIn.xyz);
    vec3 linOut = mix(
            srgbIn.xyz / vec3(12.92),
            pow((srgbIn.xyz + vec3(0.055)) / vec3(1.055), vec3(2.4)),
            bLess
        );
    return vec4(linOut, srgbIn.w);
}

// Fetch normal from normal map or fallback to interpolated normal
vec3 getNormal(MaterialData material, int normalMap)
{
    vec3 N = normalize(inNormal);

    if (normalMap < 0 || material.normalTextureIndex < 0) {
        return N;
    }

    // Tangent basis
    vec3 T = normalize(inTangent);
    vec3 B = normalize(inBitTangent);

    // Optional: ensure orthonormal TBN (for high precision)
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt
    B = normalize(cross(N, T)); // Recompute bitangent for accuracy

    mat3 TBN = mat3(T, B, N);

    vec2 uv = (material.normalTextureSet == 0 ? inUV0 : inUV1);
    vec3 tangentNormal = texture(textures[normalMap], uv).xyz * 2.0 - 1.0;

    return normalize(TBN * tangentNormal);
}

void main()
{
    MaterialData material = materials[inMaterialIndex];

    // 1) Base color (albedo)
    vec4 baseColorLinear = material.baseColorFactor;
    if (material.baseColorTextureIndex >= 0) {
        baseColorLinear *= SRGBtoLINEAR(
                texture(textures[material.baseColorTextureIndex],
                    (material.baseColorTextureSet == 0 ? inUV0 : inUV1))
            );
    }

    // Handle alpha‐masking
    if (material.alphaMask > 0.5) {
        if (baseColorLinear.a < material.alphaMaskCutoff) {
            discard;
        }
    }

    // 2) Roughness & metallic
    float roughness = material.roughnessFactor;
    float metallic = material.metallicFactor;
    // If there is a metallicRoughness texture:
    if (material.physicalDescriptorTextureIndex >= 0) {
        vec4 mrSample = texture(
                textures[material.physicalDescriptorTextureIndex],
                (material.physicalDescriptorTextureSet == 0 ? inUV0 : inUV1)
            );
        roughness = clamp(mrSample.g * roughness, 0.04, 1.0);
        metallic = clamp(mrSample.b * metallic, 0.0, 1.0);
    }

    // 3) Emissive
    vec3 emissiveCol = material.emissiveFactor.rgb * material.emissiveStrength;
    if (material.emissiveTextureIndex >= 0) {
        emissiveCol *= SRGBtoLINEAR(
                texture(textures[material.emissiveTextureIndex],
                    (material.emissiveTextureSet == 0 ? inUV0 : inUV1))
            ).rgb;
    }

    vec3 N = getNormal(material, material.normalTextureIndex);

    outAlbedo = vec4(baseColorLinear.rgb, baseColorLinear.a);
    N = getNormal(material, material.normalTextureIndex);
    vec3 n01 = N * 0.5 + 0.5; // pack into 0..1
    outNormalRoughness = vec4(n01, roughness);
    outMaterialParams = vec4(emissiveCol, metallic);
    outPosition = vec4(inWorldPos, 1.0);

    // outAlbedo = vec4(1);
    // outNormalRoughness = vec4(1);
    // outMaterialParams = vec4(1);
    // outPosition = vec4(1);
}
