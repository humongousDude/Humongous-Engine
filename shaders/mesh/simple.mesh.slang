import "../includes/inputs.slang";

struct TaskPayload {
    uint globalMeshletIndex;
    uint drawDataIndex;
    uint instanceOffset;
    uint instanceCount;
};

struct MeshOut
{
    float4 position : SV_Position;
    float4 color    : COLOR0; 
};

#define MAX_MESHLET_VERTICES 64
#define MAX_MESHLET_PRIMITIVES 124 

[[numthreads(32, 1, 1)]]
[[shader("mesh")]]
[outputtopology("triangle")] // Specify output topology
void main(
    const TaskPayload payload : payload,

    int3 gl_LocalInvocationID : SV_GroupThreadID,

    out vertices MeshOut verts[MAX_MESHLET_VERTICES],
    out indices uint3 prims[MAX_MESHLET_PRIMITIVES]
)
{
    uint globalMeshletIndex = payload.globalMeshletIndex;
    uint drawDataIndex = payload.drawDataIndex;
    // uint instanceOffset = payload.instanceOffset; // Not directly used here, but accessible via payload
    // uint instanceCount = payload.instanceCount; // Not directly used here, but accessible via payload

    Meshlet meshlet = meshlets[globalMeshletIndex];

    DrawData currentDrawData = drawData[drawDataIndex];
    InstanceData currentInstanceData = instData[currentDrawData.instanceOffset]; // Use the instance offset from DrawData

    // CRUCIAL: Set the number of vertices and primitives to be output by this mesh workgroup.
    // This tells the hardware how many elements in `verts` and `prims` are valid.
    SetMeshOutputCounts(meshlet.vertexCount, meshlet.primitiveCount);

    // Process Vertices
    // Each thread processes one vertex if its ID is within the meshlet's vertex count
    if (gl_LocalInvocationID.x < meshlet.vertexCount)
    {
        uint localVertexIdx = meshletVertices[meshlet.indexOffset + gl_LocalInvocationID.x];
        Vertex vertex = globalVertices[localVertexIdx];

        // Basic transformation: Model-View-Projection
        float4 transformedPos = mul(ubo.projectionView, mul(currentInstanceData.modelMatrix, vertex.position));

        // Assign the computed vertex data directly to the output array
        verts[gl_LocalInvocationID.x].position = transformedPos;
        verts[gl_LocalInvocationID.x].color = vertex.color;
    }

    // Process Primitives (Triangles)
    // Each thread processes one primitive if its ID is within the meshlet's primitive count
    if (gl_LocalInvocationID.x < meshlet.primitiveCount)
    {
        // Get the packed primitive indices for the current triangle
        uint packedIndices = meshletPrimitives[meshlet.primitiveID + gl_LocalInvocationID.x];

        // Unpack the 3 local vertex indices (0-255 range within the meshlet's vertex buffer)
        uint idx0 = (packedIndices >> 0) & 0xFF;   // First 8 bits
        uint idx1 = (packedIndices >> 8) & 0xFF;   // Next 8 bits
        uint idx2 = (packedIndices >> 16) & 0xFF;  // Next 8 bits

        // Assign the primitive indices directly to the output array
        prims[gl_LocalInvocationID.x] = uint3(idx0, idx1, idx2);
    }
}
