
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
    uint materialID;
    uint localNodeIndex;
    uint isSkinned;
    uint isMorphed;
    uint instanceOffset;
    uint pad0[3];
};

struct InstanceData {
    float4x4 modelMatrix;
    uint modelID;
    uint globalNodeIndex;
    uint jointStart;
    uint morphStart;
};

[[vk::binding(0, 0)]]cbuffer UBO {
    float4x4 projection;
    float4x4 invProjection;
    float4x4 view;
    float4x4 invView;
    float4x4 projectionView;
    float4 cameraPos;
    float pad0;
}  ubo;

[[vk::binding(2, 1)]] StructuredBuffer<float4x4> node;
[[vk::binding(3, 1)]] StructuredBuffer<Vertex> globalVertices;
[[vk::binding(4, 1)]] StructuredBuffer<float4x4> joints;
[[vk::binding(5, 1)]] StructuredBuffer<float> morphs;
[[vk::binding(0, 2)]] StructuredBuffer<DrawData> drawData;
[[vk::binding(1, 2)]] StructuredBuffer<InstanceData> instData;
[[vk::binding(0, 3)]] RWStructuredBuffer<uint> debugData;
