import "../includes/inputs.slang";

static const uint WORKGROUP_SIZE = 64;

[[vk::binding(2, 2)]] StructuredBuffer<MeshletDrawData> meshDrawData;

[numthreads(WORKGROUP_SIZE, 1, 1)]
[shader("amplification")]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint drawID : SV_DrawIndex)
{
    MeshletDrawData mdd = meshDrawData[drawID];

    TaskPayload payload;
    payload.drawDataIndex = mdd.drawDataIndex;
    payload.instanceIndex = mdd.instanceOffset;
    payload.instanceCount = mdd.instanceCount;
    payload.meshletCount = mdd.meshletCount;
    payload.meshletOffset = mdd.meshletIndex;
    
    // Each thread within the workgroup will process a specific meshlet-instance pair.
    // The total number of tasks to dispatch is meshletCount * instanceCount.
    uint numTasks = mdd.meshletCount * mdd.instanceCount;
    
    // Dispatch one meshlet for each task. The total number of tasks is `numTasks`.
    DispatchMesh(numTasks, 1, 1, payload);
}
