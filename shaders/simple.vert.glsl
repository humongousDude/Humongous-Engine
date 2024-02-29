#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "includes/input_structures.glsl"

layout(location = 0) out vec2 outUV0;
layout(location = 1) out vec2 outUV1;
layout(location = 2) out vec4 outColor;
layout(location = 3) out vec3 worldPosition;
layout(location = 4) out vec3 outNormal;
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
    VertexBuffer vertexBuffer;
} mnv;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
    vec3 camPos;
} ubo;

layout (set = 4, binding = 0) uniform UBONode {
    mat4 matrix;
    // mat4 jointMatrix[MAX_NUM_JOINTS];
    // float jointCount;
} node;

void main()
{
    Vertex v = mnv.vertexBuffer.vertices[gl_VertexIndex];

    vec4 locPos = ubo.projection * ubo.view * mnv.modelMatrix * node.matrix * vec4(v.position, 1.0);

    gl_Position = locPos;

    worldPosition = (mnv.modelMatrix * vec4(v.position, 1.0)).xyz;
    outNormal = normalize(transpose(inverse(mat3(mnv.modelMatrix * node.matrix))) * v.normal);

    outUV0 = v.uv1; outUV1 = v.uv2; outColor = v.color; camPos = ubo.camPos;
}
