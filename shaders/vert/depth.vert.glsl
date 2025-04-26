#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

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
    uint id;
} mnv;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
    mat4 projectionView;
    vec3 cameraPos;
} ubo;

layout(set = 4, binding = 0) readonly buffer NodeID {
    uint nodeID;
} ids;

// TODO : make this work again
layout(set = 5, binding = 0) readonly buffer UBONode {
    mat4 matrix[];
    // mat4 jointMatrix[MAX_NUM_JOINTS];
    // float jointCount;
} node;

layout(set = 7, binding = 0) buffer DebugData
{
    uint draws;
} debug;

void main()
{
    Vertex v = mnv.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = ubo.projectionView * mnv.modelMatrix * node.matrix[gl_DrawID] * vec4(v.position, 1.0);
}
