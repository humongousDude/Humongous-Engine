#version 450

#extension GL_EXT_buffer_reference : enable

layout(location = 0) out vec3 outWorldDir;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 invProjection;
    mat4 view;
    mat4 invView;
    mat4 projectionView;
    vec3 cameraPos;
} ubo;

void main() {
    vec2 positions[6] = vec2[](
            vec2(1.0, 1.0),
            vec2(-1.0, 1.0),
            vec2(1.0, -1.0),
            vec2(1.0, -1.0),
            vec2(-1.0, 1.0),
            vec2(-1.0, -1.0)
        );

    vec2 pos = positions[gl_VertexIndex];

    vec4 ndc = vec4(pos, 0.0, 1.0);

    gl_Position = ndc;
    vec4 clipCoords = vec4(pos.xy, 1.0, 1.0);
    vec4 viewDir = ubo.invProjection * clipCoords;
    vec3 viewSpaceDir = normalize(viewDir.xyz / viewDir.w);
    outWorldDir = mat3(ubo.invView) * viewSpaceDir;
}
