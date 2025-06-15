struct Vertex {
    vec4 position;
    vec4 normal;
    vec4 tangent;
    vec4 bitTangent;
    vec4 uv1;
    vec4 uv2;
    vec4 color;
};

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 invProjection;
    mat4 view;
    mat4 invView;
    mat4 projectionView;
    vec3 cameraPos;
} ubo;

layout(set = 1, binding = 2) readonly buffer UBONode {
    mat4 matrix[];
} node;

struct DrawData {
    mat4 modelMatrix;
    uint modelID;
    uint materialID;
    uint nodeIndex;
};

layout(set = 2, binding = 0) readonly buffer rrawData {
    DrawData drawData[];
} drawData;

layout(set = 3, binding = 0) readonly buffer Vertices {
    Vertex vertices[];
} globalVertices;

layout(set = 4, binding = 0) buffer DebugData
{
    uint draws;
} debug;
