[[vk::binding(0, 0)]] Sampler2D depthBuffer;

struct BoundingBox {
    float4 min;
    float4 max;
    int valid;
};

struct BoundingData {
    BoundingBox boundingBox;
    uint id;
};

[[vk::binding(1, 0)]] StructuredBuffer<BoundingData> objectData; // set = 0, binding = 1

struct VisibilityResultSet {
    uint objId;
    bool visible;
};

[[vk::binding(2, 0)]] RWStructuredBuffer<VisibilityResultSet> results;

[[vk::binding(3, 0)]] cbuffer Matricies { 
    float4x4 projection;
    float4x4 view;
    float4x4 projectionView;
    float3 camPos;
    float padding_camPos;
} matricies;

[[vk::binding(4, 0)]] cbuffer RendererData { 
    float2 screenSize;
    float2 padding_screenSize;
} rendererData;

[[vk::push_constant]]
cbuffer PC {
    uint objCount;
} pc;

[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 gl_GlobalInvocationID : SV_DispatchThreadID)
{
    uint index = gl_GlobalInvocationID.x;

    if (index >= pc.objCount) {
        return;
    }

    BoundingData currentObject = objectData[NonUniformResourceIndex(index)];
    results[NonUniformResourceIndex(index)].objId = currentObject.id;
    results[NonUniformResourceIndex(index)].visible = false;

    float4 corners[8];
    corners[0] = float4(currentObject.boundingBox.min.x, currentObject.boundingBox.min.y, currentObject.boundingBox.min.z, 1.0f);
    corners[1] = float4(currentObject.boundingBox.max.x, currentObject.boundingBox.min.y, currentObject.boundingBox.min.z, 1.0f);
    corners[2] = float4(currentObject.boundingBox.min.x, currentObject.boundingBox.max.y, currentObject.boundingBox.min.z, 1.0f);
    corners[3] = float4(currentObject.boundingBox.max.x, currentObject.boundingBox.max.y, currentObject.boundingBox.min.z, 1.0f);
    corners[4] = float4(currentObject.boundingBox.min.x, currentObject.boundingBox.min.y, currentObject.boundingBox.max.z, 1.0f);
    corners[5] = float4(currentObject.boundingBox.max.x, currentObject.boundingBox.min.y, currentObject.boundingBox.max.z, 1.0f);
    corners[6] = float4(currentObject.boundingBox.min.x, currentObject.boundingBox.max.y, currentObject.boundingBox.max.z, 1.0f);
    corners[7] = float4(currentObject.boundingBox.max.x, currentObject.boundingBox.max.y, currentObject.boundingBox.max.z, 1.0f);

    if (currentObject.boundingBox.valid == 0) {
        return;
    }

    float4 clip_space_corners[8];
    uint behind_camera_count = 0;
    for (int i = 0; i < 8; ++i) {
        clip_space_corners[i] = mul(matricies.projectionView, corners[i]);
        if (clip_space_corners[i].w < 0.0f) {
            behind_camera_count++;
        }
    }

    if (behind_camera_count == 8) {
        return;
    }

    for (int plane = 0; plane < 6; ++plane) {
        uint corners_out = 0;
        for (int i = 0; i < 8; ++i) {
            float dot_product;
            if (plane == 0) dot_product = clip_space_corners[i].w + clip_space_corners[i].x;
            else if (plane == 1) dot_product = clip_space_corners[i].w - clip_space_corners[i].x;
            else if (plane == 2) dot_product = clip_space_corners[i].w + clip_space_corners[i].y;
            else if (plane == 3) dot_product = clip_space_corners[i].w - clip_space_corners[i].y;
            else if (plane == 4) dot_product = clip_space_corners[i].w - clip_space_corners[i].z;
            else dot_product = clip_space_corners[i].z;

            if (dot_product < 0.0f) {
                corners_out++;
            }
        }
        if (corners_out == 8) {
            return;
        }
    }

    float2 min_screen_pos = float2(1.0f, 1.0f);
    float2 max_screen_pos = float2(-1.0f, -1.0f);

    float closest_z = 0.0f;

    for (int i = 0; i < 8; ++i) {
        if (clip_space_corners[i].w > 0.0f) { 
            float3 ndc = clip_space_corners[i].xyz / clip_space_corners[i].w;

            closest_z = max(closest_z, ndc.z);

            min_screen_pos = min(min_screen_pos, ndc.xy);
            max_screen_pos = max(max_screen_pos, ndc.xy);
        }
    }
    
    min_screen_pos = clamp(min_screen_pos, -1.0f, 1.0f);
    max_screen_pos = clamp(max_screen_pos, -1.0f, 1.0f);

    if (min_screen_pos.x >= max_screen_pos.x || min_screen_pos.y >= max_screen_pos.y) {
        return;
    }

    float2 uv_min = min_screen_pos * 0.5f + 0.5f;
    float2 uv_max = max_screen_pos * 0.5f + 0.5f;

    float2 screen_bbox_size = (uv_max - uv_min) * rendererData.screenSize;
    float max_dim = max(screen_bbox_size.x, screen_bbox_size.y);
    float mip_level = floor(log2(max_dim));

    uint depthMips, width, height, levels;
    depthBuffer.GetDimensions(depthMips, width, height, levels);
    mip_level = clamp(mip_level, 0.0f, float(depthMips - 1));

    float occluder_z1 = depthBuffer.SampleLevel(uv_min, mip_level).r;
    float occluder_z2 = depthBuffer.SampleLevel(uv_max, mip_level).r;
    float occluder_z3 = depthBuffer.SampleLevel(float2(uv_min.x, uv_max.y), mip_level).r;
    float occluder_z4 = depthBuffer.SampleLevel(float2(uv_max.x, uv_min.y), mip_level).r;

    float max_occluder_z = max(max(occluder_z1, occluder_z2), max(occluder_z3, occluder_z4));

    if (closest_z >= max_occluder_z) {
        results[NonUniformResourceIndex(index)].visible = true;
    }
}
