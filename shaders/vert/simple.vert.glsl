#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "../includes/inputs.vert"

layout(location = 0) out vec2 outUV0;
layout(location = 1) out vec2 outUV1;
layout(location = 2) out vec4 outColor;
layout(location = 3) out vec3 worldPosition;
layout(location = 4) out vec3 outNormal;
layout(location = 5) out vec3 outTangent;
layout(location = 6) out vec3 outBiTangent;
layout(location = 7) out uint materialID;

void main()
{
    Vertex v = globalVertices.vertices[gl_VertexIndex];
    uint nodeIndex = drawData.drawData[gl_DrawID].nodeIndex;
    mat4 nodeTransform = node.matrix[nodeIndex];
    vec4 locPos = ubo.projectionView * drawData.drawData[gl_DrawID].modelMatrix * nodeTransform * vec4(v.position.xyz, 1.0);
    gl_Position = locPos;

    worldPosition = (drawData.drawData[gl_DrawID].modelMatrix * node.matrix[nodeIndex] * vec4(v.position.xyz, 1.0)).xyz;

    mat3 normalMat = mat3(drawData.drawData[gl_DrawID].modelMatrix * nodeTransform);
    outNormal = normalize(normalMat * v.normal.xyz);

    outTangent = normalize(mat3(drawData.drawData[gl_DrawID].modelMatrix) * v.tangent.xyz);
    outBiTangent = normalize(mat3(drawData.drawData[gl_DrawID].modelMatrix) * v.bitTangent.xyz);

    outUV0 = v.uv1.xy;
    outUV1 = v.uv2.xy;
    outColor = v.color;
    materialID = drawData.drawData[gl_DrawID].materialID;

    // atomicAdd(debug.draws, 1);
}
