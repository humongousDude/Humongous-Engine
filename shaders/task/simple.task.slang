import "../includes/inputs.slang";

struct TaskPayload {
    uint globalMeshletIndex;
    uint drawDataIndex;
    uint instanceOffset;
    uint instanceCount;
};

[[numthreads(1, 1, 1)]]
[[shader("amplification")]]
void main(
    uint3 gl_GlobalInvocationID : SV_DispatchThreadID,
    out TaskPayload payload : payload
)
{
    uint currentMeshletRelativeIndex = gl_GlobalInvocationID.x;
    uint globalMeshletIndex = pc.meshletOffset + currentMeshletRelativeIndex;

    payload.globalMeshletIndex = globalMeshletIndex;
    payload.drawDataIndex = pc.drawDataIndex;
    payload.instanceOffset = pc.instanceOffset;
    payload.instanceCount = pc.instanceCount;

    DispatchMesh(1, 1, 1, payload);
}
