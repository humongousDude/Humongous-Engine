#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(local_size_x = 64) in;

layout(set = 0, binding = 0) uniform sampler2D depthBuffer;

struct BoundingBox {
    vec3 min;
    float padding1;
    vec3 max;
    float padding2;
    vec4 corners[8];
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
    VisibilityResultSet visible[];
};

layout(push_constant) uniform PC {
    uint objCount;
} pc;

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

void main() {
    uint id = gl_GlobalInvocationID.x;
    uint idx = nonuniformEXT(id);
    if (idx >= pc.objCount) return;

    BoundingData bb = objectData.data[idx];
    visible[idx].objId = bb.id;

    if (bb.boundingBox.valid == 0) {
        visible[idx].visible = false;
        return;
    }

    vec4 clip_space_corners[8];
    bool corner_in_front[8];
    uint corners_in_front_count = 0;

    // --- Step 1: Project all corners and classify them ---
    for (int i = 0; i < 8; ++i) {
        clip_space_corners[i] = matricies.projectionView * bb.boundingBox.corners[i];
        if (clip_space_corners[i].w > 0) {
            corner_in_front[i] = true;
            corners_in_front_count++;
        } else {
            corner_in_front[i] = false;
        }
    }

    // --- Step 2: Handle trivial culling cases ---
    if (corners_in_front_count == 0) {
        // All corners are behind the camera, object is not visible.
        visible[idx].visible = false;
        return;
    }

    // --- Step 3: Calculate screen-space bounds and nearest Z ---
    // (This is the key corrected section)
    vec2 minScreenNDC = vec2(1.0);
    vec2 maxScreenNDC = vec2(-1.0);
    float nearestNdcZ = 0.0; // Reverse-Z: near is 1, far is 0. Init to far.

    if (corners_in_front_count == 8) {
        // --- FAST PATH: All corners are in front ---
        // This is the common case for objects fully in view.
        for (int i = 0; i < 8; ++i) {
            vec3 ndc = clip_space_corners[i].xyz / clip_space_corners[i].w;
            minScreenNDC = min(minScreenNDC, ndc.xy);
            maxScreenNDC = max(maxScreenNDC, ndc.xy);
            nearestNdcZ = max(nearestNdcZ, ndc.z);
        }
    } else {
        // --- SLOW PATH: Object straddles the near plane ---
        // We MUST be conservative here.
        minScreenNDC = vec2(-1.0);
        maxScreenNDC = vec2(1.0);

        // We still need to find the nearest Z of the vertices that ARE in front.
        for (int i = 0; i < 8; ++i) {
            if (corner_in_front[i]) {
                vec3 ndc = clip_space_corners[i].xyz / clip_space_corners[i].w;
                nearestNdcZ = max(nearestNdcZ, ndc.z);
            }
        }
    }

    // Clamp the final NDC box to the screen boundaries.
    minScreenNDC = clamp(minScreenNDC, -1.0, 1.0);
    maxScreenNDC = clamp(maxScreenNDC, -1.0, 1.0);

    // If the clamped box is invalid, object is off-screen.
    if (minScreenNDC.x >= maxScreenNDC.x || minScreenNDC.y >= maxScreenNDC.y) {
        visible[idx].visible = false;
        return;
    }

    // --- Step 4: The rest of the shader proceeds with correct inputs ---
    vec2 minUV = minScreenNDC * 0.5 + 0.5;
    vec2 maxUV = maxScreenNDC * 0.5 + 0.5;

    // ... (LOD calculation and sampling loop remains the same) ...
    vec2 uvBoxSize = maxUV - minUV;
    vec2 screenPixelSize = uvBoxSize * rendererData.screenSize;
    float baseLod = floor(log2(max(1.0, max(screenPixelSize.x, screenPixelSize.y))));
    float targetLod = clamp(baseLod, 0.0, float(textureQueryLevels(depthBuffer) - 1));

    bool isVisible = false;
    const int gridSize = 8;
    vec2 sampleStep = uvBoxSize / (gridSize > 1 ? (gridSize - 1) : 1.0);

    for (int y = 0; y < gridSize; ++y) {
        for (int x = 0; x < gridSize; ++x) {
            vec2 sampleUV = minUV + vec2(x, y) * sampleStep;
            float occluderDepth = textureLod(depthBuffer, sampleUV, targetLod).r;

            if (nearestNdcZ >= occluderDepth - 0.00001) {
                isVisible = true;
                break;
            }
        }
        if (isVisible) break;
    }

    visible[idx].visible = isVisible;
}
