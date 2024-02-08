#version 450

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
} ubo;

layout(location = 0) in vec3 inColor;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(inColor, 1.0);
}
