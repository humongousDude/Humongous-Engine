import "../includes/inputs.slang";

struct TaskPayload {
    uint globalMeshletIndex;
    uint drawDataIndex;
    uint instanceOffset;
    uint instanceCount;
};

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

[numthreads(32, 1, 1)]
[shader("mesh")]
[outputtopology("triangle")]
void main(
    OutputVertices<OutVertex, MAX_MESHLET_VERTS> outVertices,
    OutputIndices<uint3, MAX_MESHLET_PRIMS> outIndices,
)
{
    // Minimal safe emission so SPIR-V keeps the decorated outputs.
    // Replace with your real meshlet vertex fetch/transform code.

    outVertices[0].position   = float4(-1.0, -1.0, 0.0, 1.0);
    outVertices[1].position   = float4( 1.0, -1.0, 0.0, 1.0);
    outVertices[2].position   = float4( 0.0,  1.0, 0.0, 1.0);
    for (uint i = 0; i < 3; ++i)
    {
        outVertices[i].uv0       = float2(0.0, 0.0);
        outVertices[i].uv1       = float2(0.0, 0.0);
        outVertices[i].color     = float4(1,1,1,1);
        outVertices[i].normal    = float3(0,0,1);
        outVertices[i].tangent   = float3(1,0,0);
        outVertices[i].bitangent = float3(0,1,0);
        outVertices[i].materialID = 0;
    }

    outIndices[0] = uint3(0, 1, 2);

    SetMeshOutputCounts(3, 1);
}
