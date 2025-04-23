// BUG: up/down edges of screen have quite big areas where objects are reported not occluded incorrectly
// BUG: when adding a closer object suddenly, some objects can flicker

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

layout(std140, set = 0, binding = 1) readonly buffer ObjectData {
    BoundingBox boundingBoxes[];
};

layout(set = 0, binding = 2) buffer VisibilityResults {
    bool visible[];
};

layout(set = 0, binding = 3) uniform Matricies {
    mat4 projection;
    mat4 view;
    mat4 projectionView;
    vec3 camPos;
} matricies;

layout(std140, set = 0, binding = 4) uniform RendererData {
    vec2 screenSize;
} rendererData;

float getDepth(vec2 screenCoords) {
    // Normalize before clamping
    vec2 texCoords = screenCoords / rendererData.screenSize;
    texCoords = clamp(texCoords, vec2(0.0), vec2(1.0));
    return texture(depthBuffer, texCoords).r;
}

float linearizeDepth(float ndcDepth, float near, float far) {
    return (2.0 * near * far) / (ndcDepth * (near - far) + far + near);
}

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id >= boundingBoxes.length()) return;

    BoundingBox bb = boundingBoxes[id];
    if (bb.valid == 0) {
        visible[id] = false;
        return;
    }

    vec3 screenCorners[8];
    bool occluded = true;

    for (int i = 0; i < 8; i++)
    {
        vec4 viewSpace = matricies.view * (bb.corners[i]);
        vec4 clipSpace = matricies.projection * viewSpace;
        vec3 ndcSpace = clipSpace.xyz / clipSpace.w;
        screenCorners[i].x = (ndcSpace.x + 1.0) * 0.5 * rendererData.screenSize.x;
        screenCorners[i].y = (1.0 - ndcSpace.y) * 0.5 * rendererData.screenSize.y;
        screenCorners[i].z = ndcSpace.z;

        vec2 cornerScreenPos = screenCorners[i].xy;
        float cornerDepth = linearizeDepth(screenCorners[i].z, 0.1f, 1000);
        float depthBufferDepth = linearizeDepth(getDepth(cornerScreenPos), 0.1f, 1000);

        if (cornerDepth < depthBufferDepth + 0.001f) {
            occluded = false;
            break;
        }
    }

    visible[id] = !occluded;
}
