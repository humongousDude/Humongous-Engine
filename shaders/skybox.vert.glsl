#version 450

#extension GL_EXT_buffer_reference : enable

layout(location = 0) out vec3 outUV;

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

layout(push_constant) uniform PUSH
{
    VertexBuffer vertexBuffer;
} push;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
    vec3 camPos;
} ubo;

void main()
{
    Vertex v = push.vertexBuffer.vertices[gl_VertexIndex];

    // For a skybox, the view matrix should only affect rotation, not translation.
    // Remove the translation component from the view matrix.
    mat4 viewRotOnly = ubo.view;
    viewRotOnly[3][0] = 0.0;
    viewRotOnly[3][1] = 0.0;
    viewRotOnly[3][2] = 0.0;

    gl_Position = ubo.projection * viewRotOnly * vec4(v.position, 1.0);

    // Use the vertex position as the texture coordinate for the skybox.
    outUV = v.position;
}
