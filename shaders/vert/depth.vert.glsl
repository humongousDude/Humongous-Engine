#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

struct Vertex {
    vec4 position;
    vec4 normal;
    vec4 tangent;
    vec4 bitTangent;
    vec4 uv1;
    vec4 uv2;
    vec4 color;
};

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 invProjection;
    mat4 view;
    mat4 invView;
    mat4 projectionView;
    vec3 cameraPos;
} ubo;

layout(set = 3, binding = 2) readonly buffer UBONode {
    mat4 matrix[];
    // mat4 jointMatrix[MAX_NUM_JOINTS];
    // float jointCount;
} node;

struct DrawData {
    mat4 modelMatrix;
    uint modelID;
    uint materialID;
    uint nodeIndex;
};

layout(set = 4, binding = 0) readonly buffer rrawData {
    DrawData drawData[];
} drawData;

layout(set = 5, binding = 0) readonly buffer Vertices {
    Vertex vertices[];
} globalVertices;

layout(set = 6, binding = 0) buffer DebugData
{
    uint draws;
} debug;

void main()
{
    Vertex v = globalVertices.vertices[gl_VertexIndex];
    uint nodeIndex = drawData.drawData[gl_DrawID].nodeIndex;

    mat4 nodeTransform = node.matrix[nodeIndex];

    vec4 locPos = ubo.projectionView * drawData.drawData[gl_DrawID].modelMatrix * nodeTransform * vec4(v.position.xyz, 1.0);
    gl_Position = locPos;
}
