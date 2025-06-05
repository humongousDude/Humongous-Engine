#version 450

#extension GL_EXT_buffer_reference : enable

layout(location = 0) out vec3 outUV;
layout(location = 1) out vec3 outWorldDir;

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
    mat4 invProjection;
    mat4 view;
    mat4 invView;
    mat4 projectionView;
    vec3 cameraPos;
} ubo;

void main() {
    vec2 positions[6] = vec2[](
            vec2(-1.0, -1.0),
            vec2(1.0, -1.0),
            vec2(-1.0, 1.0),
            vec2(-1.0, 1.0),
            vec2(1.0, -1.0),
            vec2(1.0, 1.0)
        );

    vec2 uvs[6] = vec2[](
            vec2(0.0, 0.0),
            vec2(1.0, 0.0),
            vec2(0.0, 1.0),
            vec2(1.0, 1.0),
            vec2(0.0, 0.0),
            vec2(1.0, 0.0)
        );
    // Use gl_VertexIndex to select from quad
    vec2 pos = positions[gl_VertexIndex];
    vec2 uv = uvs[gl_VertexIndex];

    // Clip-space position
    gl_Position = vec4(pos, 1.0, 1.0);

    // Convert to NDC (normalized device coordinates)
    vec4 ndc = vec4(pos, 1.0, 1.0);

    // Remove translation from view matrix
    mat4 viewNoTrans = mat4(mat3(ubo.view));
    mat4 invProjView = inverse(ubo.projection * viewNoTrans);

    // Unproject to world space
    vec4 world = invProjView * ndc;
    outWorldDir = normalize(world.xyz / world.w);
}
