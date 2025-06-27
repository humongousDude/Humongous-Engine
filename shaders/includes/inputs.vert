struct Vertex {
    vec4 position;
    vec4 normal;
    vec4 tangent;
    vec4 bitTangent;
    vec4 uv1;
    vec4 uv2;
    vec4 color;
    ivec4 joint0;
    vec4 weight0;
    vec4 targetPos0;
    vec4 targetPos1;
    vec4 targetPos2;
    vec4 targetPos3;
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
    uint jointStart;
    uint morphStart;
    int isSkinned;
    int isMorphed;
};

layout(set = 2, binding = 0) readonly buffer rrawData {
    DrawData drawData[];
} drawData;

layout(std140, set = 1, binding = 3) readonly buffer Vertices {
    Vertex vertices[];
} globalVertices;

layout(set = 1, binding = 4) readonly buffer joinMatrices {
    mat4 matrices[];
} joints;

layout(set = 1, binding = 5) readonly buffer morphWeights {
    float weights[];
} morphs;

layout(set = 3, binding = 0) buffer DebugData
{
    uint draws;
} debug;
