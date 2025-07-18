//
// layout(location = 0) in vec3 inWorldDir;
//
// layout(set = 1, binding = 0) uniform samplerCube cubemap;
//
// layout(location = 0) out vec4 outColor;

[[vk::binding(0,1)]] SamplerCube cubemap;

[shader("fragment")]
void main(
    float3 inWorldDir : WORLDDIR0,
    out float4 outColor : SV_Target0
){
    outColor = cubemap.Sample(normalize(inWorldDir));
}
