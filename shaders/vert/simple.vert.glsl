#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec2 outUV0;
layout(location = 1) out vec2 outUV1;
layout(location = 2) out vec4 outColor;
layout(location = 3) out vec3 worldPosition;
layout(location = 4) out vec3 outNormal;
layout(location = 5) out vec3 cameraPos;

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
    uint modelID;
} mnv;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
    mat4 projectionView;
    vec3 cameraPos;
} ubo;

layout(set = 3, binding = 2) readonly buffer UBONode {
    mat4 matrix[];
    // mat4 jointMatrix[MAX_NUM_JOINTS];
    // float jointCount;
} node;

layout(set = 4, binding = 0) writeonly buffer DebugData
{
    uint draws;
} debug;

void main()
{
    Vertex v = mnv.vertexBuffer.vertices[gl_VertexIndex];
    uint nodeid = gl_DrawID + mnv.modelID;

    vec4 locPos = ubo.projectionView * mnv.modelMatrix * node.matrix[nodeid] * vec4(v.position, 1.0);
    gl_Position = locPos;

    worldPosition = (mnv.modelMatrix * vec4(v.position, 1.0)).xyz;
    outNormal = normalize(transpose(inverse(mat3(mnv.modelMatrix * node.matrix[nodeid]))) * v.normal);

    outUV0 = v.uv1;
    outUV1 = v.uv2;
    cameraPos = ubo.cameraPos;
    outColor = v.color;

    atomicAdd(debug.draws, 1);
}
