[[vk::binding(0, 0)]] Sampler2D sourceMip;
[[vk::binding(1, 0)]] RWTexture2D<float> destMip;

[[vk::push_constant]]
cbuffer PushConstants {
    int mipLevel;
} pushConst;

[numthreads(8, 8, 1)]
[shader("compute")] 
void main(uint3 gl_GlobalInvocationID : SV_DispatchThreadID)
{
    uint2 pos = gl_GlobalInvocationID.xy;
    uint width, height;
    destMip.GetDimensions(width, height);

    if (pos.x >= width || pos.y >= height) {
        return;
    }

    float depth = sourceMip.Sample((float2(pos) + 0.5f) / float2(width, height)).x;

    destMip[pos] = depth;
}
