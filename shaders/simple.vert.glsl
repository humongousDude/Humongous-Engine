#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "includes/input_structures.glsl"

layout(location = 0) out vec2 outUV0;
layout(location = 1) out vec2 outUV1;
layout(location = 2) out vec4 outColor;
layout(location = 3) out vec3 worldPosition;
layout(location = 4) out vec3 worldNormal;
layout(location = 5) out vec3 camPos;

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv1;
    vec2 uv2;
    vec4 color;
};

layout(buffer_reference, std140) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(push_constant) uniform MNV 
{
    mat4 modelMatrix;
    // vec4 padding0;
    // mat3 normalMatrix;
    VertexBuffer vertexBuffer;
} mnv;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
    vec3 camPos;
} ubo;

void main()
{
    Vertex v = mnv.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = ubo.projection * ubo.view * mnv.modelMatrix * vec4(v.position, 1.0);

    worldPosition = (mnv.modelMatrix * vec4(v.position, 1.0)).xyz;
    // worldNormal = mnv.normalMatrix * v.normal;
    worldNormal = vec3(0.0, 0.0, 1.0);

    outUV0 = v.uv1; outUV1 = v.uv2; outColor = v.color; camPos = ubo.camPos;
}
