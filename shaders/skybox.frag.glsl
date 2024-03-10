#version 450

layout(location = 0) in vec3 inUV;

layout(set = 1, binding = 0) uniform samplerCube cubemap;

layout(location = 0) out vec4 outColor;

vec3 Uncharted2Tonemap(vec3 color)
{
    float A = 0.1500000059604644775390625;
    float B = 0.5;
    float C = 0.100000001490116119384765625;
    float D = 0.20000000298023223876953125;
    float E = 0.0199999995529651641845703125;
    float F = 0.300000011920928955078125;
    float W = 11.19999980926513671875;
    return (((color * ((color * A) + vec3(C * B))) + vec3(D * E)) / ((color * ((color * A) + vec3(B))) + vec3(D * F))) - vec3(E / F);
}

vec4 tonemap(vec4 color)
{
    vec3 param = color.xyz * 5 /* uboParams.exposure */ ;
    vec3 outcol = Uncharted2Tonemap(param);
    vec3 param_1 = vec3(11.19999980926513671875);
    outcol *= (vec3(1.0) / Uncharted2Tonemap(param_1));
    return vec4(pow(outcol, vec3(1.0 / 5 /* uboParams.gamma */ )), color.w);
}

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
    vec3 bLess = step(vec3(0.040449999272823333740234375), srgbIn.xyz);
    vec3 linOut = mix(srgbIn.xyz / vec3(12.9200000762939453125), pow((srgbIn.xyz + vec3(0.054999999701976776123046875)) / vec3(1.05499994754791259765625), vec3(2.400000095367431640625)), bLess);
    return vec4(linOut, srgbIn.w);
}

void main()
{
    vec4 param = textureLod(cubemap, inUV, 1.5);
    vec4 param_1 = tonemap(param);
    vec3 color = SRGBtoLINEAR(param_1).xyz;
    outColor = vec4(color * 1.0, 1.0);
}
