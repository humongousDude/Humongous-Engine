#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform samplerCube cubeMap;

// Material bindings

// Textures

layout(set = 1, binding = 0) uniform sampler2D colorMap;
layout(set = 1, binding = 1) uniform sampler2D physicalDescriptorMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D aoMap;
layout(set = 1, binding = 4) uniform sampler2D emissiveMap;

// Properties

#include "includes/shadermaterial.glsl"

layout(std430, set = 2, binding = 0) buffer SSBO
{
    ShaderMaterial materials[];
};

layout(push_constant) uniform Push
{
    uint materialIndex;
} push;

void main()
{
    ShaderMaterial shad = materials[push.materialIndex];

    outFragColor = vec4(shad.baseColorFactor.rgb, 1.0f);
}
