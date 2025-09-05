import "../includes/inputs.slang";

struct OutVertex{
    float4 position      : SV_Position; 
    float2 uv0          : TEXCOORD0;    
    float2 uv1          : TEXCOORD1;    
    float4 color        : COLOR0;       
    float3 worldPosition: POSITION0;    
    float3 normal       : NORMAL0;      
    float3 tangent      : TANGENT0;     
    float3 bitangent    : BITANGENT0;   
    uint   materialID   : MATERIALID0;  
};


static const uint MAX_MESHLET_VERTS = 64;
static const uint MAX_MESHLET_PRIMS = 126;
static const uint GROUP_SIZE = 32;

[numthreads(GROUP_SIZE, 1, 1)]
[shader("mesh")]
[outputtopology("triangle")]
void main(
    in payload TaskPayload payload,
    uint3 gid : SV_GroupID,
    uint tid : SV_GroupThreadID,
    OutputVertices<OutVertex, MAX_MESHLET_VERTS> outVertices,
    OutputIndices<uint3, MAX_MESHLET_PRIMS> outIndices
)
{
    // The GroupID.x tells us which meshlet-instance pair we are.
    // We need to unpack this into a specific meshlet index and instance index.
    uint totalMeshlets = payload.meshletCount;
    uint totalInstances = payload.instanceCount;
    
    uint taskIndex = gid.x;
    
    uint meshletIndex = taskIndex % totalMeshlets;
    uint instanceIndex = taskIndex / totalMeshlets;

    Meshlet m = meshlets[payload.meshletOffset + meshletIndex];
    DrawData d = drawData[payload.drawDataIndex];
    InstanceData inst = instData[payload.instanceIndex + instanceIndex];

    float4x4 nodeTransform = node[d.localNodeIndex + inst.globalNodeIndex];
    float4x4 modelTransform = mul(inst.modelMatrix, nodeTransform);
    float3x3 normalMat = (float3x3)modelTransform;

    uint vcount = min(m.vertexCount, MAX_MESHLET_VERTS);
    uint pcount = min(m.primitiveCount, MAX_MESHLET_PRIMS);

    for (uint i = tid; i < vcount; i += GROUP_SIZE)
    {
        uint meshletVertIndex = meshletVertices[m.localVertexOffset + i];
        uint globalVertIndex   = d.vertexOffset + meshletVertIndex;
        Vertex vin = globalVertices[globalVertIndex];

        float4 worldPos4 = mul(modelTransform, vin.position);
        float4 clip = mul(ubo.projectionView, worldPos4);

        outVertices[i].position      = clip;
        outVertices[i].worldPosition = worldPos4.xyz;
        outVertices[i].normal        = normalize(mul(normalMat, vin.normal.xyz));
        outVertices[i].tangent       = normalize(mul(normalMat, vin.tangent.xyz));
        outVertices[i].bitangent     = normalize(mul(normalMat, vin.bitTangent.xyz));
        outVertices[i].uv0           = vin.uv1.xy;
        outVertices[i].uv1           = vin.uv2.xy;
        outVertices[i].color         = vin.color;
        outVertices[i].materialID    = d.materialID;
    }

    uint primBase = m.localIndexOffset;
    for (uint pi = tid; pi < pcount; pi += GROUP_SIZE)
    {
        uint base = primBase + pi * 3u;
        uint b0 = uint(meshletPrimitives[base + 0u]);
        uint b1 = uint(meshletPrimitives[base + 1u]);
        uint b2 = uint(meshletPrimitives[base + 2u]);

        if (b0 >= vcount) b0 = 0;
        if (b1 >= vcount) b1 = 0;
        if (b2 >= vcount) b2 = 0;

        outIndices[pi] = uint3(b0, b1, b2);
    }


    if (tid == 0)
    {
        SetMeshOutputCounts(vcount, pcount);
    }
}
