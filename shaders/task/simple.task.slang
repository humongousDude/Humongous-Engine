import "../includes/inputs.slang";

static const uint WORKGROUP_SIZE = 64;

[[vk::binding(2, 2)]] StructuredBuffer<MeshletDrawInfo> meshDrawData;

[numthreads(WORKGROUP_SIZE, 1, 1)]
[shader("amplification")]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint drawID : SV_DrawIndex)
{
    MeshletDrawInfo mdd = meshDrawData[drawID];

    TaskPayload payload;
    payload.drawDataIndex = mdd.drawDataIndex;
    payload.instanceIndex = mdd.instanceOffset;
    payload.instanceCount = mdd.instanceCount;
    payload.meshletCount = mdd.meshletCount;
    payload.meshletOffset = mdd.meshletIndex;
    
    uint numTasks = mdd.meshletCount * mdd.instanceCount;
    DispatchMesh(numTasks, 1, 1, payload);
}
