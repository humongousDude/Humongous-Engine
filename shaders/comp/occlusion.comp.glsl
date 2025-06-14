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

    vec2 minScreenNDC = vec2(1.0);
    vec2 maxScreenNDC = vec2(-1.0);
    float maxZ = 0.0;
    bool fullyBehind = true;

    for (int i = 0; i < 8; ++i) {
        vec4 worldPos = bb.boundingBox.corners[i];
        vec4 clipSpace = matricies.projectionView * worldPos;

        if (clipSpace.w > 0) {
            fullyBehind = false;
            vec3 ndc = clipSpace.xyz / clipSpace.w;
            minScreenNDC = min(minScreenNDC, ndc.xy);
            maxScreenNDC = max(maxScreenNDC, ndc.xy);

            maxZ = max(maxZ, ndc.z);
        }
    }

    if (fullyBehind ||
            maxScreenNDC.x < -1.0 || maxScreenNDC.y < -1.0 ||
            minScreenNDC.x > 1.0 || minScreenNDC.y > 1.0) {
        visible[idx].visible = false;
        return;
    }

    minScreenNDC = clamp(minScreenNDC, -1.0, 1.0);
    maxScreenNDC = clamp(maxScreenNDC, -1.0, 1.0);

    vec2 minUV = minScreenNDC * 0.5 + 0.5;
    vec2 maxUV = maxScreenNDC * 0.5 + 0.5;

    vec2 uvBoxSize = maxUV - minUV;

    vec2 screenPixelSize = uvBoxSize * rendererData.screenSize;
    float baseLod = floor(log2(max(1.0, max(screenPixelSize.x, screenPixelSize.y))));
    float targetLod = clamp(baseLod, 0.0, float(textureQueryLevels(depthBuffer) - 1));

    bool isVisible = false;
    const int gridSize = 8;
    vec2 sampleStep = uvBoxSize / (gridSize - 1);

    for (int y = 0; y < gridSize; ++y) {
        for (int x = 0; x < gridSize; ++x) {
            vec2 sampleUV = minUV + vec2(x, y) * sampleStep;
            float occluderDepth = textureLod(depthBuffer, sampleUV, targetLod).r;

            if (maxZ >= occluderDepth - 0.00001) {
                isVisible = true;
                break;
            }
        }
        if (isVisible) {
            break;
        }
    }

    visible[idx].visible = isVisible;
}
