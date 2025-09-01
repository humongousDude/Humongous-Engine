import "../includes/inputs.slang";

struct TaskPayload {
    uint globalMeshletIndex;
    uint drawDataIndex;
    uint instanceOffset;
    uint instanceCount;
};

groupshared TaskPayload payload;

[numthreads(1, 1, 1)]
[shader("amplification")]
void main(uint3 gl_GlobalInvocationID : SV_DispatchThreadID)
{
    payload.globalMeshletIndex = pc.meshletOffset + gl_GlobalInvocationID.x;
    payload.drawDataIndex      = pc.drawDataIndex;
    payload.instanceOffset     = pc.instanceOffset;
    payload.instanceCount      = pc.instanceCount;

    DispatchMesh(1, 1, 1, payload);
}
