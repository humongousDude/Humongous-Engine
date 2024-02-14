#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "includes/input_structures.glsl"

layout(location = 0) out vec2 fragUV;

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv1;
    vec2 uv2;
};

layout(buffer_reference, std140) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(push_constant) uniform Push
{
    mat4 modelMatrix;
    VertexBuffer vertexBuffer;
} push;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
} ubo;

void main()
{
    Vertex v = push.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = ubo.projection * ubo.view * push.modelMatrix * vec4(v.position, 1.0);

    fragUV = v.uv1;
}
