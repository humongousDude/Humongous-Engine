#pragma once

// TODO: Cleanup

// based on Sascha Willems' tinyGltf vulkan example

#include "logical_device.hpp"
#include "material.hpp"
#include "texture.hpp"
#include <Eigen/Dense>

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
    n32         localVertexOffset{0};
    n32         globalVertexOffset{0};
    n32         indexCount{0};
    n32         vertexCount{0};
    f32         maxMorphDisplacement{0};
    BoundingBox boundingBox{};
    n32         id{0};

    std::vector<std::vector<Eigen::Vector3f>> morphTargetPositions; // Offsets for positions
    std::vector<std::vector<Eigen::Vector3f>> morphTargetNormals;
    std::vector<std::vector<Eigen::Vector4f>> morphTargetTangents;
    n32                                       globalWeightOffset{0};

    Material* material = nullptr;
    bool      hasIndices = false;

    n32 meshletCount{0};
    n32 meshletOffset{0};

    Primitive(n32 firstIndex, n32 indexCount, n32 vertexCount, n32 localVertexOffset, Material* material)
        : localFirstIndex(firstIndex), indexCount(indexCount), vertexCount(vertexCount), localVertexOffset(localVertexOffset), material(material),
          hasIndices(indexCount > 0)
    {
    }

    Primitive() {}
};

struct Mesh
{
    Mesh(const LogicalDevice& device, Eigen::Matrix4f matrix);
    ~Mesh();

    n32                     baseVertex = 0;
    n32                     baseIndex = 0;
    const LogicalDevice&    logicalDevice;
    std::vector<Primitive*> primitives;
    std::vector<f32>        weights;
};

struct Meshlet
{
    n32             vertexOffset;
    n32             vertexCount;
    n32             indexOffset;
    n32             primitiveCount;
    Eigen::Vector4f boundingSphere;
    n32             primitiveID;
};

class Model
{
public:
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
        InterpolationType            interpolation;
        std::vector<f32>             inputs;
        std::vector<Eigen::Vector4f> outputsVec4;
        std::vector<f32>             outputs;
        Eigen::Vector4f              CubicSplineInterpolation(size_t index, f32 time, n32 stride) const;
        void ApplyTranslation(size_t index, f32 time, std::vector<Eigen::Vector3f>& translations, n32 targetNodeIndex) const;
        void ApplyScale(size_t index, f32 time, std::vector<Eigen::Vector3f>& scales, n32 targetNodeIndex) const;
        void ApplyRotation(size_t index, f32 time, std::vector<Eigen::Quaternionf>& rotations, n32 targetNodeIndex) const;
        void ApplyMorph(size_t index, f32 time, const Primitive& targetPrimitive, std::vector<f32>& instanceWeights) const;
    };

    struct Animation
    {
        std::string                   name;
        std::vector<AnimationSampler> samplers;
        std::vector<AnimationChannel> channels;
        f32                           start = std::numeric_limits<f32>::max();
        f32                           end = std::numeric_limits<f32>::min();
    };

    struct InstanceCreationInfo
    {
        std::unordered_map<std::string, n32> animNameToIndex;
        std::unordered_map<n32, std::string> animIndexToName;
        b32                                  hasMorphTargets{false};
        b32                                  hasAnimations{false};
        std::vector<Animation>               animations;
        BoundingBox                          animatedAABB{};
        std::vector<Eigen::Matrix4f>         nodeMatrices;
        std::vector<Eigen::Matrix4f>         jointMatrices;
        std::vector<f32>                     morphTargets;
    };

    struct alignas(16) PushConstantData
    {
        Eigen::Matrix4f   model = Eigen::Matrix4f::Identity();
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
        Eigen::Vector4f baseColorFactor; // 16 bytes
        Eigen::Vector4f emissiveFactor;  // 16 bytes
        Eigen::Vector4f diffuseFactor;   // 16 bytes
        Eigen::Vector4f specularFactor;  // 16 bytes

        f32 workflow;                       // 4
        int baseColorTextureIndex;          // 4
        int baseColorTextureSet;            // 4
        int physicalDescriptorTextureIndex; // 4

        int physicalDescriptorTextureSet; // 4
        int normalTextureIndex;           // 4
        int normalTextureSet;             // 4
        int occlusionTextureIndex;        // 4

        int occlusionTextureSet;  // 4
        int emissiveTextureIndex; // 4
        int emissiveTextureSet;   // 4
        f32 metallicFactor;       // 4

        f32 roughnessFactor;  // 4
        f32 alphaMask;        // 4
        f32 alphaMaskCutoff;  // 4
        f32 emissiveStrength; // 4
    };

    struct Vertex
    {
        Eigen::Vector4f position = Eigen::Vector4f::Zero();
        Eigen::Vector4f normal = Eigen::Vector4f::Zero();
        Eigen::Vector4f tangent = Eigen::Vector4f::Zero();
        Eigen::Vector4f bitTangent = Eigen::Vector4f::Zero();
        Eigen::Vector4f uv0 = Eigen::Vector4f::Zero();
        Eigen::Vector4f uv1 = Eigen::Vector4f::Zero();
        Eigen::Vector4f color = Eigen::Vector4f::Zero();
        Eigen::Vector4i joint0 = Eigen::Vector4i::Zero();
        Eigen::Vector4f weight0 = Eigen::Vector4f::Zero();
        Eigen::Vector4f targetPos0 = Eigen::Vector4f::Zero();
        Eigen::Vector4f targetPos1 = Eigen::Vector4f::Zero();

        bool operator==(const Vertex& other) const
        {
            return position == other.position && normal == other.normal && uv0 == other.uv0 && uv1 == other.uv1 && color == other.color;
        }
    };

    // this passes
    static_assert(std::is_standard_layout<Vertex>::value, "Vertex must be standard‑layout for meshoptimizer to memcpy it correctly");

    Model(const LogicalDevice& device, const std::string& modelPath, f32 scale, const n32& handle);
    ~Model();

    std::vector<n32>& GetIndices() { return m_indices; }

    struct Dimensions
    {
        Eigen::Vector3f min = Eigen::Vector3f::Constant(FLT_MAX);
        Eigen::Vector3f max = Eigen::Vector3f::Constant(-FLT_MAX);
    };

    InstanceCreationInfo GetAnimationData() const
    {
        return {m_animNameToIndex, m_animIndexToName, m_hasMorphTargets, HasAnimations(), m_animations,
                m_animatedAABB,    m_nodeMatricies,   m_jointMatricies,  m_morphTargets};
    }
    std::vector<Eigen::Matrix4f> GetJointMatrices() const { return m_jointMatricies; }
    std::vector<f32>             GetMorphTargets() const { return m_morphTargets; }

    BoundingBox GetLocalBoundingBox() const { return m_animationTime > 0.0f ? m_animatedAABB : m_restAABB; }

    Primitive* GetPrimitive(const n32& index) const { return m_primitives[index]; }
    n32        GetHandle() const { return m_handle; }

    std::string                                       GetName() const { return m_name; }
    std::unordered_map<n32, std::vector<Primitive*>>& GetMaterialBatches() { return m_materialBatches; }

    b32 HasAnimations() const { return !m_animations.empty(); }
    b32 HasSkins() const { return !m_skins.empty(); }

    std::vector<Skin*> GetSkins() const { return m_skins; }
    std::vector<Mesh*> GetMeshes() const { return m_meshes; }
    std::vector<Node*> GetNodes() const { return m_nodes; }
    std::vector<Node*> GetLinearNodes() const { return m_linearNodes; }

    n32                           GetAnimationCount() const { return m_animations.size(); }
    const std::vector<Animation>& GetAnimations() const { return m_animations; }
    b32                           HasMorphs() const { return m_hasMorphTargets; }

    Node* FindNode(Node* parent, n32 index);
    Node* NodeFromIndex(n32 index);

private:
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

    const LogicalDevice& m_logicalDevice;

    std::vector<Node*>      m_nodes;
    std::vector<Node*>      m_linearNodes;
    std::vector<Mesh*>      m_meshes;
    std::vector<Skin*>      m_skins;
    std::vector<Primitive*> m_primitives;

    std::vector<Eigen::Matrix4f> m_jointMatricies;
    std::vector<f32>             m_morphTargets;

    std::vector<Meshlet> m_meshlets;
    std::vector<n32>     m_meshletVertices;
    std::vector<n8>      m_meshletPrimitives;

    BoundingBox m_restAABB{};
    BoundingBox m_animatedAABB{};

    std::vector<n32>                     m_textures;
    std::vector<Texture::TexSamplerInfo> m_textureSamplers;
    std::vector<Material>                m_materials;

    enum PBRWorkflows
    {
        PBR_WORKFLOW_METALLIC_ROUGHNESS = 0,
        PBR_WORKFLOW_SPECULAR_GLOSSINESS = 1
    };

    std::vector<Eigen::Matrix4f> m_nodeMatricies{};

    struct LoaderInfo
    {
        std::vector<n32>           indexBuffer;
        std::vector<Model::Vertex> vertexBuffer;
        n32                        indexPos = 0;
        n32                        vertexPos = 0;
    };

    bool m_initialized{false};
    void LoadFromFile(std::string filepath, const LogicalDevice& device, vk::Queue transferQueue, f32 scale = 1.0f);
    void Destroy(vk::Device m_device);

    void OptimizeMeshes();
    void CreateMeshlets(const LoaderInfo& loaderInfo);

    void LoadMaterialData();
    void UpdateMaterialBatches(Node* node);

    void LoadNode(Node* parent, const tinygltf::Node& node, n32 nodeIndex, const tinygltf::Model& model, LoaderInfo& loaderInfo, f32 globalscale,
                  Eigen::Matrix4f parentTransform);

    void OptimizePrimitive(Primitive* primitive, LoaderInfo& loaderInfo, std::vector<Vertex>& primitiveVertices,
                           std::vector<n32>& primitiveIndices);

    void GetNodeProps(const tinygltf::Node& node, const tinygltf::Model& model, size_t& vertexCount, size_t& indexCount);
    void LoadTextures(tinygltf::Model& gltfModel, const LogicalDevice& m_device, vk::Queue transferQueue);

    void LoadSkins(tinygltf::Model& gltfModel);
    void LoadAnimations(tinygltf::Model& gltfModel);

    vk::SamplerAddressMode GetVkWrapMode(s32 wrapMode);
    vk::Filter             GetVkFilterMode(s32 filterMode);

    void LoadTextureSamplers(tinygltf::Model& gltfModel);
    void LoadMaterials(tinygltf::Model& gltfModel);

    void CalculateRestAABB();
};
} // namespace Humongous
