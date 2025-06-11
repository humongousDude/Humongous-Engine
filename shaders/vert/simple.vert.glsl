#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec2 outUV0;
layout(location = 1) out vec2 outUV1;
layout(location = 2) out vec4 outColor;
layout(location = 3) out vec3 worldPosition;
layout(location = 4) out vec3 outNormal;
layout(location = 5) out vec3 outTangent;
layout(location = 6) out vec3 outBitTangent;
layout(location = 7) out uint materialID;

struct Vertex {
    vec4 position;
    vec4 normal;
    vec4 tangent;
    vec4 bitTangent;
    vec4 uv1;
    vec4 uv2;
    vec4 color;
};

layout(buffer_reference, std140) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

struct DrawData {
    mat4 modelMatrix;
    uint modelID;
    uint materialID;
    uint nodeIndex;
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
} node;

layout(set = 4, binding = 0) readonly buffer rrawData {
    DrawData drawData[];
} drawData;

layout(set = 5, binding = 0) readonly buffer Vertices {
    Vertex vertices[];
} globalVertices;

layout(set = 6, binding = 0) writeonly buffer DebugData
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

    worldPosition = (drawData.drawData[gl_DrawID].modelMatrix * node.matrix[nodeIndex] * vec4(v.position.xyz, 1.0)).xyz;

    mat3 normalMat = mat3(drawData.drawData[gl_DrawID].modelMatrix * nodeTransform);
    outNormal = normalize(normalMat * v.normal.xyz);

    outTangent = normalize(mat3(drawData.drawData[gl_DrawID].modelMatrix) * v.tangent.xyz);
    outBitTangent = normalize(mat3(drawData.drawData[gl_DrawID].modelMatrix) * v.bitTangent.xyz);

    outUV0 = v.uv1.xy;
    outUV1 = v.uv2.xy;
    outColor = v.color;
    materialID = drawData.drawData[gl_DrawID].materialID;

    // atomicAdd(debug.draws, 1);
}
