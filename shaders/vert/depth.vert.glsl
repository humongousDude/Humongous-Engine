#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "../includes/inputs.vert"

void main()
{
    Vertex v = globalVertices.vertices[gl_VertexIndex];
    uint nodeIndex = drawData.drawData[gl_DrawID].nodeIndex;
    mat4 nodeTransform = node.matrix[nodeIndex];
    vec4 locPos = ubo.projectionView * drawData.drawData[gl_DrawID].modelMatrix * nodeTransform * vec4(v.position.xyz, 1.0);
    gl_Position = locPos;
}
