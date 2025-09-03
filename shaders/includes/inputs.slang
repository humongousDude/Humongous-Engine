
struct Vertex {
    float4 position;
    float4 normal;
    float4 tangent;
    float4 bitTangent;
    float4 uv1;
    float4 uv2;
    float4 color;
    int4 joint0;
    float4 weight0;
    float4 targetPos0;
    float4 targetPos1;
}

struct DrawData {
    uint vertexOffset;
    uint materialID;
    uint localNodeIndex;
    uint isSkinned;
    uint isMorphed;
    uint instanceOffset;
    uint __pad0[2];
};

struct InstanceData {
    float4x4 modelMatrix;
    uint modelID;
    uint globalNodeIndex;
    uint jointStart;
    uint morphStart;
};

struct Meshlet{
    uint localVertexOffset;
    uint globalVertexOffset;
    uint vertexCount;
    uint localIndexOffset;
    uint globalIndexOffset;
    uint primitiveCount;
    uint __pad0[2];
};

struct MeshletPushConstants {
    uint meshletIndex;
    uint drawDataIndex;
    uint instanceOffset;
    uint instanceCount;
    uint meshletCount;
};

[[vk::push_constant]] MeshletPushConstants pc;

[[vk::binding(0, 0)]]cbuffer UBO {
    float4x4 projection;
    float4x4 invProjection;
    float4x4 view;
    float4x4 invView;
    float4x4 projectionView;
    float4 cameraPos;
}  ubo;

[[vk::binding(2, 1)]] StructuredBuffer<float4x4> node;
[[vk::binding(3, 1)]] StructuredBuffer<Vertex> globalVertices;
[[vk::binding(4, 1)]] StructuredBuffer<float4x4> joints;
[[vk::binding(5, 1)]] StructuredBuffer<float> morphs;
[[vk::binding(6, 1)]] StructuredBuffer<Meshlet> meshlets;
[[vk::binding(7, 1)]] StructuredBuffer<uint> meshletVertices;
[[vk::binding(8, 1)]] StructuredBuffer<uint8_t> meshletPrimitives;
[[vk::binding(0, 2)]] StructuredBuffer<DrawData> drawData;
[[vk::binding(1, 2)]] StructuredBuffer<InstanceData> instData;
[[vk::binding(0, 3)]] RWStructuredBuffer<uint> debugData;
