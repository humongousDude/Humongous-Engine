
[[vk::binding(0,0)]] cbuffer UBO {
    float4x4 projection;
    float4x4 invProjection;
    float4x4 view;
    float4x4 invView;
    float4x4 projectionView;
    float3 cameraPos;
} ubo;

[[shader("vertex")]]
void main(
    uint vertexID : SV_VertexID,
    out float3 outWorldDir : WORLDDIR0,
    out float4 outPosition : SV_Position
){
    float2 positions[6] = float2[](
            float2(1.0, 1.0),
            float2(-1.0, 1.0),
            float2(1.0, -1.0),
            float2(1.0, -1.0),
            float2(-1.0, 1.0),
            float2(-1.0, -1.0)
        );

    float2 pos = positions[vertexID];

    float4 ndc = float4(pos, 0.0, 1.0);

    outPosition = ndc;
    float4 clipCoords = float4(pos.xy, 1.0, 1.0);
    float4 viewDir = mul(ubo.invProjection , clipCoords);
    float3 viewSpaceDir = normalize(viewDir.xyz / viewDir.w);
    outWorldDir = mul(float3x3(ubo.invView) , viewSpaceDir);
}
