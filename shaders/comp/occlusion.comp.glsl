#version 460
layout(local_size_x = 64) in;

layout(set = 0, binding = 0) uniform sampler2D depthBuffer;

struct BoundingBox {
    vec4 min;
    vec4 max;
    int valid;
};

struct BoundingData {
    BoundingBox boundingBox;
    uint id;
};

layout(std430, set = 0, binding = 1) readonly buffer ObjectData {
    BoundingData data[];
} objectData;

struct VisibilityResultSet {
    uint objId;
    bool visible;
};

layout(std430, set = 0, binding = 2) writeonly buffer VisibilityResults {
    VisibilityResultSet results[];
} results;

layout(set = 0, binding = 3) uniform Matricies {
    mat4 projection;
    mat4 view;
    mat4 projectionView;
    vec3 camPos;
    float padding_camPos;
} matricies;

layout(std140, set = 0, binding = 4) uniform RendererData {
    vec2 screenSize;
    vec2 padding_screenSize;
} rendererData;

layout(push_constant) uniform PC {
    uint objCount;
} pc;

void main() {
    uint index = gl_GlobalInvocationID.x;

    if (index >= pc.objCount) {
        return;
    }

    BoundingData currentObject = objectData.data[index];
    results.results[index].objId = currentObject.id; // Set ID early
    results.results[index].visible = false; // Default to not visible

    vec4 corners[8];
    corners[0] = vec4(currentObject.boundingBox.min.x, currentObject.boundingBox.min.y, currentObject.boundingBox.min.z, 1.0f);
    corners[1] = vec4(currentObject.boundingBox.max.x, currentObject.boundingBox.min.y, currentObject.boundingBox.min.z, 1.0f);
    corners[2] = vec4(currentObject.boundingBox.min.x, currentObject.boundingBox.max.y, currentObject.boundingBox.min.z, 1.0f);
    corners[3] = vec4(currentObject.boundingBox.max.x, currentObject.boundingBox.max.y, currentObject.boundingBox.min.z, 1.0f);
    corners[4] = vec4(currentObject.boundingBox.min.x, currentObject.boundingBox.min.y, currentObject.boundingBox.max.z, 1.0f);
    corners[5] = vec4(currentObject.boundingBox.max.x, currentObject.boundingBox.min.y, currentObject.boundingBox.max.z, 1.0f);
    corners[6] = vec4(currentObject.boundingBox.min.x, currentObject.boundingBox.max.y, currentObject.boundingBox.max.z, 1.0f);
    corners[7] = vec4(currentObject.boundingBox.max.x, currentObject.boundingBox.max.y, currentObject.boundingBox.max.z, 1.0f);

    if (currentObject.boundingBox.valid == 0) {
        return;
    }

    vec4 clip_space_corners[8];
    uint behind_camera_count = 0;
    for (int i = 0; i < 8; ++i) {
        clip_space_corners[i] = matricies.projectionView * corners[i];
        if (clip_space_corners[i].w < 0.0) {
            behind_camera_count++;
        }
    }

    if (behind_camera_count == 8) {
        return; // Culled
    }

    // Frustum Culling (same as before)
    for (int plane = 0; plane < 6; ++plane) {
        uint corners_out = 0;
        for (int i = 0; i < 8; ++i) {
            float dot_product;
            if (plane == 0) dot_product = clip_space_corners[i].w + clip_space_corners[i].x;
            else if (plane == 1) dot_product = clip_space_corners[i].w - clip_space_corners[i].x;
            else if (plane == 2) dot_product = clip_space_corners[i].w + clip_space_corners[i].y;
            else if (plane == 3) dot_product = clip_space_corners[i].w - clip_space_corners[i].y;
            else if (plane == 4) dot_product = clip_space_corners[i].w - clip_space_corners[i].z; // Z is in [0,W] for reversed-Z
            else dot_product = clip_space_corners[i].z; //

            if (dot_product < 0.0) {
                corners_out++;
            }
        }
        if (corners_out == 8) {
            return; // Culled
        }
    }

    vec2 min_screen_pos = vec2(1.0);
    vec2 max_screen_pos = vec2(-1.0);

    // --- CORRECTED ---
    // Find the max Z value (closest point to camera in reversed-Z)
    float closest_z = 0.0;

    for (int i = 0; i < 8; ++i) {
        // We must clip corners against the near plane (w > 0) before perspective division
        // to avoid huge screen coordinates from points behind the camera.
        // A simple way is to ignore them for the screen bbox calculation.
        if (clip_space_corners[i].w > 0.0) {
            vec3 ndc = clip_space_corners[i].xyz / clip_space_corners[i].w;

            // CORRECTED: Find the largest Z, which is the closest point to the camera.
            closest_z = max(closest_z, ndc.z);

            min_screen_pos = min(min_screen_pos, ndc.xy);
            max_screen_pos = max(max_screen_pos, ndc.xy);
        }
    }

    // Clamp screen-space bounding box to the screen edges [-1, 1] in NDC
    min_screen_pos = clamp(min_screen_pos, -1.0, 1.0);
    max_screen_pos = clamp(max_screen_pos, -1.0, 1.0);

    if (min_screen_pos.x >= max_screen_pos.x || min_screen_pos.y >= max_screen_pos.y) {
        return; // Culled (projected to a line/point or outside screen)
    }

    // Convert NDC [-1, 1] to UV [0, 1] for texture sampling
    vec2 uv_min = min_screen_pos * 0.5 + 0.5;
    vec2 uv_max = max_screen_pos * 0.5 + 0.5;

    vec2 screen_bbox_size = (uv_max - uv_min) * rendererData.screenSize;
    float max_dim = max(screen_bbox_size.x, screen_bbox_size.y);
    float mip_level = floor(log2(max_dim));

    // --- OPTIONAL TWEAK ---
    // If culling is still too aggressive, sample from a higher-res mip.
    // This is a great value to expose as a "quality level" setting.
    // mip_level = max(0.0, mip_level - 1.0);
    mip_level = clamp(mip_level, 0, textureQueryLevels(depthBuffer) - 1);

    // --- UPGRADED SAMPLING LOGIC ---
    // Sample the 4 corners of the screen-space bounding box to get a more robust depth value.
    float occluder_z1 = textureLod(depthBuffer, uv_min, mip_level).r;
    float occluder_z2 = textureLod(depthBuffer, uv_max, mip_level).r;
    float occluder_z3 = textureLod(depthBuffer, vec2(uv_min.x, uv_max.y), mip_level).r;
    float occluder_z4 = textureLod(depthBuffer, vec2(uv_max.x, uv_min.y), mip_level).r;

    // Find the closest occluder among the 4 samples (the one with the MAX Z value).
    float max_occluder_z = max(max(occluder_z1, occluder_z2), max(occluder_z3, occluder_z4));

    // The Test: The object is visible if its closest point is closer than or at the same
    // depth as the closest occluder in the area it covers.
    if (closest_z >= max_occluder_z) {
        results.results[index].visible = true;
    }
}
