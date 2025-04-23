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
    mat4 projectionView;
    vec3 cameraPos;
} ubo;

void main()
{
    Vertex v = push.vertexBuffer.vertices[gl_VertexIndex];

    // Correct way to get view rotation only:
    mat4 viewRotOnly = mat4(mat3(ubo.view)); // Extract the 3x3 rotation part

    gl_Position = ubo.projection * viewRotOnly * vec4(v.position, 1.0);

    // Generate spherical coordinates for UVs:
    outUV = normalize(v.position); // Use normalized position as direction vector
}
