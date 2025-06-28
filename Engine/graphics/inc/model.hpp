#pragma once

// TODO: Cleanup

// based on Sascha Willems' tinyGltf vulkan example

#include "logger.hpp"
#include "logical_device.hpp"
#include "material.hpp"
#include "texture.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

// ERROR is already defined in wingdi.h and collides with a define in the Draco headers
#if defined(_WIN32) && defined(ERROR) && defined(TINYGLTF_ENABLE_DRACO)
#undef ERROR
#pragma message("ERROR constant already defined, undefining")
#endif

#include <scene.hpp>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

namespace Humongous
{
struct Primitive
{
    Node*       owner{nullptr};
    n32         localFirstIndex{0};
    n32         globalFirstIndex{0};
    n32         localVertexStart{0};
    n32         vertexOffset{0};
    n32         indexCount{0};
    n32         vertexCount{0};
    f32         maxMorphDisplacement{0};
    BoundingBox boundingBox{};

    std::vector<std::vector<glm::vec3>> morphTargetPositions; // Offsets for positions
    std::vector<std::vector<glm::vec3>> morphTargetNormals;
    std::vector<std::vector<glm::vec4>> morphTargetTangents;

    Material& material;
    bool      hasIndices;
    Primitive(n32 firstIndex, n32 indexCount, n32 vertexCount, n32 localVertexStart, Material& material)
        : localFirstIndex(firstIndex), indexCount(indexCount), vertexCount(vertexCount), localVertexStart(localVertexStart), vertexOffset(0),
          material(material), hasIndices(indexCount > 0)
    {
    }
};

struct Mesh
{
    Mesh(LogicalDevice* device, glm::mat4 matrix);
    ~Mesh();

    n32                     baseVertex = 0;
    n32                     baseIndex = 0;
    LogicalDevice*          logicalDevice;
    std::vector<Primitive*> primitives;
    std::vector<float>      weights;
};

class Model
{
private:
    struct Animation;

public:
    struct alignas(16) PushConstantData
    {
        glm::mat4         model{1.f};
        vk::DeviceAddress vertexAddress;
        n32               modelID;
    };

    struct alignas(16) MaterialIndices
    {
        n32 baseColor;
        n32 normal;
    };

    struct alignas(16) ShaderMaterial
    {
        glm::vec4 baseColorFactor; // 16 bytes
        glm::vec4 emissiveFactor;  // 16 bytes
        glm::vec4 diffuseFactor;   // 16 bytes
        glm::vec4 specularFactor;  // 16 bytes

        float workflow;                       // 4
        int   baseColorTextureIndex;          // 4
        int   baseColorTextureSet;            // 4
        int   physicalDescriptorTextureIndex; // 4

        int physicalDescriptorTextureSet; // 4
        int normalTextureIndex;           // 4
        int normalTextureSet;             // 4
        int occlusionTextureIndex;        // 4

        int   occlusionTextureSet;  // 4
        int   emissiveTextureIndex; // 4
        int   emissiveTextureSet;   // 4
        float metallicFactor;       // 4

        float roughnessFactor;  // 4
        float alphaMask;        // 4
        float alphaMaskCutoff;  // 4
        float emissiveStrength; // 4
    };
    // static_assert(sizeof(ShaderMaterial) % 16 == 0, "Must be 16-byte aligned for std430");

    struct alignas(16) Vertex
    {
        glm::vec4  position{0};
        glm::vec4  normal{0};
        glm::vec4  tangent{0};
        glm::vec4  bitTangent{0};
        glm::vec4  uv0{0};
        glm::vec4  uv1{0};
        glm::vec4  color{0};
        glm::ivec4 joint0{0};
        glm::vec4  weight0{0};
        glm::vec4  targetPos0{0};
        glm::vec4  targetPos1{0};
        glm::vec4  targetPos2{0};
        glm::vec4  targetPos3{0};

        bool operator==(const Vertex& other) const
        {
            return position == other.position && normal == other.normal && uv0 == other.uv0 && uv1 == other.uv1 && color == other.color;
        }
    };

    Model(LogicalDevice* device, const std::string& modelPath, float scale, const n32& handle);
    ~Model();

    std::vector<n32>& GetIndices() { return m_indices; }

    struct Dimensions
    {
        glm::vec3 min = glm::vec3(FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX);
    };

    BoundingBox GetLocalBoundingBox() { return m_animationTime > 0.0f ? m_animatedAABB : m_restAABB; }

    std::vector<glm::mat4> GetMatrixVector();

    std::string                                       GetName() const { return m_name; }
    std::unordered_map<n32, std::vector<Primitive*>>& GetMaterialBatches() { return m_materialBatches; }
    void                                              UpdateAnimation(float time);

    b32 HasAnimations() const { return !m_animations.empty(); }
    b32 HasSkins() const { return !m_skins.empty(); }

    void Update();

    void SetAnimation(const std::string_view& animName)
    {
        auto it = m_animNameToIndex.find(animName.data());
        if(it == m_animNameToIndex.end())
        {
            HGWARN("Invalid animName. Skipping update");
            return;
        }
        m_currentAnimationIndex = it->second;
        m_animationTime = 0;
    };

    void SetAnimation(const n32& index)
    {
        m_currentAnimationIndex = index;
        m_animationTime = 0;
    }

    void PlayAnimation()
    {
        m_playAnimation = true;
        m_animationTime = 0;
    };
    void StopAnimation()
    {
        m_playAnimation = false;
        m_animationTime = 0;
    };
    void PauseAnimation() { m_playAnimation = false; };
    void UnPauseAnimation() { m_playAnimation = true; };

    n32                           GetAnimationCount() const { return m_animations.size(); }
    std::string                   GetCurrentAnimationName() const { return m_animIndexToName.at(m_currentAnimationIndex); }
    f32                           GetAnimationTime() const { return m_animationTime; }
    const std::vector<Animation>& GetAnimations() const { return m_animations; }
    b32                           HasMorphs() const { return m_hasMorphTargets; }

private:
    struct AnimationChannel
    {
        enum PathType
        {
            TRANSLATION,
            ROTATION,
            SCALE,
            WEIGHTS
        };
        PathType path;
        Node*    node;
        n32      samplerIndex;
    };

    struct AnimationSampler
    {
        enum InterpolationType
        {
            LINEAR,
            STEP,
            CUBICSPLINE
        };
        InterpolationType      interpolation;
        std::vector<float>     inputs;
        std::vector<glm::vec4> outputsVec4;
        std::vector<float>     outputs;
        glm::vec4              CubicSplineInterpolation(size_t index, f32 time, n32 stride);
        void                   Translate(size_t index, f32 time, Node* node);
        void                   Scale(size_t index, f32 time, Node* node);
        void                   Rotate(size_t index, f32 time, Node* node);
        void                   Morph(size_t index, f32 time, Node* node);
    };

    struct Animation
    {
        std::string                   name;
        std::vector<AnimationSampler> samplers;
        std::vector<AnimationChannel> channels;
        float                         start = std::numeric_limits<float>::max();
        float                         end = std::numeric_limits<float>::min();
    };
    std::vector<Animation>               m_animations;
    std::unordered_map<std::string, n32> m_animNameToIndex;
    std::unordered_map<n32, std::string> m_animIndexToName;
    n32                                  m_currentAnimationIndex = -1;
    f32                                  m_animationTime = 0;
    b32                                  m_updateAnimation{false};
    b32                                  m_playAnimation{false};
    b32                                  m_hasMorphTargets = false;

    std::string m_name = "";
    n32         m_handle{0};

    std::vector<n32>    m_indices;
    std::vector<Vertex> m_vertices;

    std::unordered_map<n32, std::vector<Primitive*>> m_materialBatches;

    LogicalDevice* m_logicalDevice;

    std::vector<Node*> m_nodes;
    std::vector<Node*> m_linearNodes;
    std::vector<Mesh*> m_meshes;
    std::vector<Skin*> m_skins;

    BoundingBox m_restAABB{};
    BoundingBox m_animatedAABB{};

    Texture                              m_emptyTexture;
    std::vector<n32>                     m_textures;
    std::vector<Texture::TexSamplerInfo> m_textureSamplers;
    std::vector<Material>                m_materials;

    enum PBRWorkflows
    {
        PBR_WORKFLOW_METALLIC_ROUGHNESS = 0,
        PBR_WORKFLOW_SPECULAR_GLOSSINESS = 1
    };

    std::vector<glm::mat4> m_nodeMatricies{};

    struct LoaderInfo
    {
        std::vector<n32>           indexBuffer;
        std::vector<Model::Vertex> vertexBuffer;
        n32                        indexPos = 0;
        n32                        vertexPos = 0;
    };

    bool m_initialized{false};
    void LoadFromFile(std::string filepath, LogicalDevice* device, vk::Queue transferQueue, float scale = 1.0f);
    void Destroy(vk::Device m_device);

    void LoadMaterialData();

    void UpdateUBO(Node* node, glm::mat4 matrix);
    void UpdateMaterialBatches(Node* node);

    void LoadNode(Node* parent, const tinygltf::Node& node, n32 nodeIndex, const tinygltf::Model& model, LoaderInfo& loaderInfo, float globalscale,
                  glm::mat4 parentTransform);
    void GetNodeProps(const tinygltf::Node& node, const tinygltf::Model& model, size_t& vertexCount, size_t& indexCount);
    void LoadTextures(tinygltf::Model& gltfModel, LogicalDevice* m_device, vk::Queue transferQueue);

    Node* FindNode(Node* parent, n32 index);
    Node* NodeFromIndex(n32 index);

    void LoadSkins(tinygltf::Model& gltfModel);
    void LoadAnimations(tinygltf::Model& gltfModel);

    vk::SamplerAddressMode GetVkWrapMode(s32 wrapMode);
    vk::Filter             GetVkFilterMode(s32 filterMode);

    void LoadTextureSamplers(tinygltf::Model& gltfModel);
    void LoadMaterials(tinygltf::Model& gltfModel);

    void CalculateRestAABB();

    void UpdateAnimatedAABB();
};
} // namespace Humongous
