import "../includes/inputs.slang";


static const uint MAX_VISIBLE_MESHLETS_PER_DRAW = 4096;
static const uint WORKGROUP_SIZE = 64;

bool IsSphereInFrustum(float3 center, float radius, float4 planes[6])
{
    for (int i = 0; i < 6; ++i) {
        if (dot(planes[i].xyz, center) + planes[i].w < -radius) return false;
    }
    return true;
}

[[vk::binding(2, 2)]] StructuredBuffer<MeshletDrawInfo> meshDrawData;

groupshared uint visibleMeshletIndices[MAX_VISIBLE_MESHLETS_PER_DRAW];
groupshared uint visibleCount;


[numthreads(WORKGROUP_SIZE, 1, 1)]
[shader("amplification")]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint drawID : SV_DrawIndex)
{
    MeshletDrawInfo mdd = meshDrawData[drawID];
    DrawData d = drawData[mdd.drawDataIndex];


    if (dispatchThreadID.x == 0) {
        visibleCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    for (uint i = dispatchThreadID.x; i < mdd.instanceCount * mdd.meshletCount; i += WORKGROUP_SIZE)
    {
        uint instanceIdx = i / mdd.meshletCount;
        uint meshletIdx = i % mdd.meshletCount;

        InstanceData inst = instData[mdd.instanceOffset + instanceIdx];
        
        Meshlet m = meshlets[mdd.meshletIndex + meshletIdx];

        float4x4 nodeTransform = node[d.localNodeIndex + inst.globalNodeIndex];
        float4x4 modelTransform = mul(inst.modelMatrix, nodeTransform);
        
        float3 worldCenter = mul(modelTransform, float4(m.boundingSphere.xyz, 1.0)).xyz;
        float scaledRadius = m.boundingSphere.w * modelTransform[0][0]; 

        bool inFrustum = IsSphereInFrustum(worldCenter, scaledRadius, ubo.frustumPlanes);
        if(inFrustum)
        {
            uint storageIndex;
            InterlockedAdd(visibleCount, 1, storageIndex);

            if (storageIndex < MAX_VISIBLE_MESHLETS_PER_DRAW) {
                visibleMeshletIndices[storageIndex] = (instanceIdx << 16) | meshletIdx;
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (dispatchThreadID.x == 0 && visibleCount > 0)
    {
        uint baseOffset;
        InterlockedAdd(visibleMeshletCounter[0], visibleCount, baseOffset);

        for(uint i = 0; i < visibleCount; i++) {
            visibleMeshletIndicesOut[baseOffset + i] = visibleMeshletIndices[i];
        }

        TaskPayload payload;
        payload.drawDataIndex = mdd.drawDataIndex;
        payload.visibleMeshletListOffset = baseOffset;
        payload.instanceCount = mdd.instanceCount;
        payload.meshletCount = mdd.meshletCount;
        payload.meshletOffset = mdd.meshletIndex;
        payload.instanceIndex = mdd.instanceOffset;
        DispatchMesh(visibleCount, 1, 1, payload);
    }
}
