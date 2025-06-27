#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "../includes/inputs.vert"

void main()
{
    // Fetch base vertex data
    Vertex v = globalVertices.vertices[gl_VertexIndex];
    DrawData currentDraw = drawData.drawData[gl_DrawID];

    // --- 1. MORPH TARGETS ---
    vec4 localPos = v.position;
    vec3 localNormal = v.normal.xyz;
    vec3 localTangent = v.tangent.xyz;

    if (currentDraw.isMorphed > 0)
    {
        uint morphOffset = currentDraw.morphStart;
        localPos.xyz += morphs.weights[morphOffset + 0] * v.targetPos0.xyz;
        localPos.xyz += morphs.weights[morphOffset + 1] * v.targetPos1.xyz;
        localPos.xyz += morphs.weights[morphOffset + 2] * v.targetPos2.xyz;
        localPos.xyz += morphs.weights[morphOffset + 3] * v.targetPos3.xyz;
    }

    // --- 2. SKINNED ANIMATION ---
    mat4 skinMatrix = mat4(1.0); // Start with identity matrix
    if (currentDraw.isSkinned > 0)
    {
        uint jointIndexOffset = currentDraw.jointStart;
        ivec4 jointIndices = v.joint0;
        vec4 jointWeights = v.weight0;

        skinMatrix = jointWeights.x * joints.matrices[jointIndexOffset + jointIndices.x];
        skinMatrix += jointWeights.y * joints.matrices[jointIndexOffset + jointIndices.y];
        skinMatrix += jointWeights.z * joints.matrices[jointIndexOffset + jointIndices.z];
        skinMatrix += jointWeights.w * joints.matrices[jointIndexOffset + jointIndices.w];

        // Apply skinning deformation to the (potentially morphed) vertex attributes
        localPos = skinMatrix * localPos;
        localNormal = mat3(skinMatrix) * localNormal;
        localTangent = mat3(skinMatrix) * localTangent;
    }

    // --- 3. STANDARD TRANSFORMATIONS ---
    uint nodeIndex = currentDraw.nodeIndex;
    mat4 nodeTransform = node.matrix[nodeIndex];
    mat4 modelTransform = currentDraw.modelMatrix * nodeTransform;

    // Final vertex position
    gl_Position = ubo.projectionView * modelTransform * localPos;
}
