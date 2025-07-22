import "../includes/inputs.slang";

[shader("vertex")]
void main(
    uint vertexID : SV_VertexID,
    uint instanceID : SV_InstanceID,
    uint drawID : SV_DrawIndex,

    out float4 outPosition : SV_Position,
)
{
    Vertex  v = globalVertices[vertexID]; 
    DrawData currentDraw = drawData[drawID];
    uint globalInstanceIndex = currentDraw.instanceOffset + instanceID;
    InstanceData currentInstance = instData[globalInstanceIndex];
    float4x4 modelMatrix = currentInstance.modelMatrix;

    float4 localPos = float4(v.position);
    float3 localNormal = v.normal.xyz;
    float3 localTangent = v.tangent.xyz;

    if (currentDraw.isMorphed > 0)
    {
        uint morphOffset = currentInstance.morphStart;
        localPos.xyz += morphs[morphOffset + 0] * v.targetPos0.xyz;
        localPos.xyz += morphs[morphOffset + 1] * v.targetPos1.xyz;
    }

    float4x4 skinMatrix = float4x4(1.0); 
    if (currentDraw.isSkinned > 0)
    {
        uint jointIndexOffset = currentInstance.jointStart;
        int4 jointIndices = v.joint0;
        float4 jointWeights = v.weight0;

        skinMatrix = jointWeights.x * joints[jointIndexOffset + jointIndices.x];
        skinMatrix += jointWeights.y * joints[jointIndexOffset + jointIndices.y];
        skinMatrix += jointWeights.z * joints[jointIndexOffset + jointIndices.z];
        skinMatrix += jointWeights.w * joints[jointIndexOffset + jointIndices.w];

        localPos = mul(skinMatrix, localPos); 
        localNormal = mul((float3x3)skinMatrix, localNormal);
        localTangent = mul((float3x3)skinMatrix, localTangent);
    }

    uint nodeIndex = currentDraw.localNodeIndex + currentInstance.globalNodeIndex;
    float4x4 nodeTransform = node[nodeIndex];
    float4x4 modelTransform = mul(modelMatrix, nodeTransform); 

    float4 finalPosition = mul(ubo.projectionView, mul(modelTransform, localPos));
    outPosition = finalPosition;
  
}
