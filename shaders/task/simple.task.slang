import "../includes/inputs.slang";

struct TaskPayload {
    uint globalMeshletIndex;
    uint drawDataIndex;
    uint instanceIndex;
    uint instanceCount;
};

groupshared TaskPayload payload;

[numthreads(1, 1, 1)]
[shader("amplification")]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    payload.globalMeshletIndex = pc.meshletIndex + dispatchThreadID.x;
    payload.drawDataIndex      = pc.drawDataIndex;
    payload.instanceIndex      = pc.instanceOffset + dispatchThreadID.y;

    DispatchMesh(1, 1, 1, payload);
}
