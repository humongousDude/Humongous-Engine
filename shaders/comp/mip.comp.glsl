#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_SourceMip;

layout(set = 0, binding = 1, r32f) uniform writeonly image2D u_DestMip;

layout(push_constant) uniform pc {
    int mipLevel;
} pushConst;

void main() {
    ivec2 destCoords = ivec2(gl_GlobalInvocationID.xy);

    ivec2 destSize = imageSize(u_DestMip);

    if (destCoords.x >= destSize.x || destCoords.y >= destSize.y) {
        return;
    }

    if (pushConst.mipLevel == 0) {
        float value = texelFetch(u_SourceMip, destCoords, 0).r;
        imageStore(u_DestMip, destCoords, vec4(value, 0.0, 0.0, 0.0));
        return;
    }

    ivec2 sourceCoords = destCoords * 2;

    float d0 = texelFetch(u_SourceMip, sourceCoords, 0).r; // Top-Left
    float d1 = texelFetch(u_SourceMip, sourceCoords + ivec2(1, 0), 0).r; // Top-Right
    float d2 = texelFetch(u_SourceMip, sourceCoords + ivec2(0, 1), 0).r; // Bottom-Left
    float d3 = texelFetch(u_SourceMip, sourceCoords + ivec2(1, 1), 0).r; // Bottom-Right

    float maxDepth = max(max(d0, d1), max(d2, d3));

    imageStore(u_DestMip, destCoords, vec4(maxDepth, 0.0, 0.0, 0.0));
}
