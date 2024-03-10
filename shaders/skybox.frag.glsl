#version 450

layout(location = 0) in vec3 inUV;

layout(set = 1, binding = 0) uniform samplerCube cubemap;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(cubemap, vec3(1.0, 0.0, 0.0));
}
