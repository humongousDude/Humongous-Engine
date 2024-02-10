#version 450

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = texture(tex, inUV);
}
