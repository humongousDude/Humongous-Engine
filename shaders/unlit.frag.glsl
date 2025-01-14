#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec2 inUV0;
layout(location = 1) in vec2 inUV1;
layout(location = 2) in vec4 inColor0;
layout(location = 3) in vec3 inWorldPos;
layout(location = 4) in vec3 inNormal;
layout(location = 5) in vec3 inCamPos;

layout(location = 0) out vec4 outColor;

// Textures

layout(set = 2, binding = 0) uniform sampler2D colorMap;
layout(set = 2, binding = 1) uniform sampler2D physicalDescriptorMap;
layout(set = 2, binding = 2) uniform sampler2D normalMap;
layout(set = 2, binding = 3) uniform sampler2D aoMap;
layout(set = 2, binding = 4) uniform sampler2D emissiveMap;

// Properties

#include "includes/shadermaterial.glsl"

layout(std430, set = 3, binding = 0) readonly buffer SSBO
{
    ShaderMaterial materials[];
};

layout(push_constant) uniform Push
{
    layout(offset = 80) uint materialIndex;
} push;

layout(set = 1, binding = 0) uniform UBOParams {
    vec4 lightDir;
    float exposure;
    float gamma;
    float prefilteredCubeMipLevels;
    float scaleIBLAmbient;
    float debugViewInputs;
    float debugViewEquation;
} uboParams;

#include "includes/srgbtolinear.glsl"

void main()
{
    ShaderMaterial material = materials[push.materialIndex];
    vec4 baseColor;

    vec2 uv = material.baseColorTextureSet == 0 ? inUV0 : inUV1;
    float lod = textureQueryLod(colorMap, uv).x;

    if (material.baseColorTextureSet > -1) {
        baseColor = SRGBtoLINEAR(textureLod(colorMap, uv, pow(lod, 0.5))) * material.baseColorFactor;
    } else {
        baseColor = material.baseColorFactor;
    }

    if (baseColor.a < material.alphaMaskCutoff) {
        discard;
    }

    outColor = baseColor;
}
