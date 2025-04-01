#pragma once

// TODO: Cleanup

// based on Sascha Willems' tinyGltf vulkan example

#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool_growable.hpp"
#include "logical_device.hpp"
#include "material.hpp"
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
    Node*     m_owner;
    n32       m_firstIndex;
    n32       m_indexCount;
    n32       m_vertexCount;
    Material& m_material;
    bool      m_hasIndices;
    Primitive(n32 firstIndex, n32 indexCount, n32 vertexCount, Material& material);
};

struct Mesh
{
    Mesh(LogicalDevice* m_device, glm::mat4 matrix);
    ~Mesh();

    LogicalDevice*          m_device;
    std::vector<Primitive*> m_primitives;

    struct UniformBuffer
    {
        Buffer                 uniformBuffer;
        VkDescriptorBufferInfo descriptorInfo;
        VkDescriptorSet        descriptorSet = VK_NULL_HANDLE;
    } m_uniformBuffer;

    struct UniformBlock
    {
        glm::mat4 matrix{1.f};
    } m_uniformBlock;
};

class Model
{
public:
    struct alignas(16) PushConstantData
    {
        glm::mat4       model{1.f};
        VkDeviceAddress vertexAddress;
        n32             id;
    };

    struct alignas(16) Vertex
    {
        alignas(16) glm::vec3 position; // 12 bytes (aligned to 16 bytes)
        alignas(16) glm::vec3 normal;   // 12 bytes (aligned to 16 bytes)
        alignas(8) glm::vec2 uv0;       // 8 bytes
        alignas(8) glm::vec2 uv1;       // 8 bytes
        alignas(16) glm::vec4 color;    // 16 bytes

        bool operator==(const Vertex& other) const
        {
            return position == other.position && normal == other.normal && uv0 == other.uv0 && uv1 == other.uv1 && color == other.color;
        }
    };

    Model(LogicalDevice* device, const std::string& modelPath, float scale);
    ~Model();

    Buffer& GetVertexBuffer() { return m_vertices; }

    void Init(DescriptorSetLayout* materialLayout, DescriptorSetLayout* nodeLayout, DescriptorSetLayout* materialBufferLayout,
              DescriptorPoolGrowable* imagePool, DescriptorPoolGrowable* uniformPool, DescriptorPoolGrowable* storagePool);

    void Draw(VkCommandBuffer commandBuffer, VkPipelineLayout& pipelineLayout);

    struct Dimensions
    {
        glm::vec3 min = glm::vec3(FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX);
    };

    BoundingBox GetLocalBoundingBox() { return m_localAABB; }

private:
    struct IndirectDrawCommand
    {
        n32 indexCount;
        n32 instanceCount;
        n32 firstIndex;
        n32 vertexOffset;
        n32 firstInstance;
    };

    Buffer m_vertices;
    Buffer m_indices;

    LogicalDevice* m_device;

    std::vector<Node*> m_nodes;
    std::vector<Node*> m_linearNodes;

    Texture                                          m_emptyTexture;
    std::vector<Texture>                             m_textures;
    std::vector<Texture::TexSamplerInfo>             m_textureSamplers;
    std::vector<Material>                            m_materials;
    std::unordered_map<n32, std::vector<Primitive*>> m_materialBatches;

    VkDescriptorSet m_descriptorSetMaterials{VK_NULL_HANDLE};
    enum PBRWorkflows
    {
        PBR_WORKFLOW_METALLIC_ROUGHNESS = 0,
        PBR_WORKFLOW_SPECULAR_GLOSSINESS = 1
    };

    struct alignas(16) ShaderMaterial
    {
        glm::vec4 baseColorFactor;
        glm::vec4 emissiveFactor;
        glm::vec4 diffuseFactor;
        glm::vec4 specularFactor;
        float     workflow;
        int       colorTextureSet;
        int       PhysicalDescriptorTextureSet;
        int       normalTextureSet;
        int       occlusionTextureSet;
        int       emissiveTextureSet;
        float     metallicFactor;
        float     roughnessFactor;
        float     alphaMask;
        float     alphaMaskCutoff;
        float     emissiveStrength;
    };
    Buffer m_shaderMaterialBuffer;

    void                                                               SetupIndirectDrawBuffer();
    Buffer                                                             m_indirectDrawBuffer;
    std::unordered_map<n32, std::vector<VkDrawIndexedIndirectCommand>> m_indirectCommands;
    std::vector<VkDrawIndexedIndirectCommand>                          m_debugCommands;

    struct LoaderInfo
    {
        n32*    indexBuffer;
        Vertex* vertexBuffer;
        size_t  indexPos = 0;
        size_t  vertexPos = 0;
    };

    BoundingBox CalculateLocalAABB(LoaderInfo& loaderInfo) const;
    BoundingBox m_localAABB{};

    bool m_initialized{false};
    void LoadFromFile(std::string filename, LogicalDevice* device, VkQueue transferQueue, float scale = 1.0f);
    void Destroy(VkDevice m_device);

    // TODO: maybe move the shader material buffer and this out?
    // maybe only write at draw time?
    void CreateMaterialBuffer();
    void UpdateShaderMaterialBuffer(Node* node);

    void UpdateUBO(Node* node, glm::mat4 matrix);
    void UpdateMaterialBatches(Node* node);

    void LoadNode(Node* parent, const tinygltf::Node& node, n32 nodeIndex, const tinygltf::Model& model, LoaderInfo& loaderInfo, float globalscale,
                  glm::mat4 parentTransform);
    void GetNodeProps(const tinygltf::Node& node, const tinygltf::Model& model, size_t& vertexCount, size_t& indexCount);
    void LoadTextures(tinygltf::Model& gltfModel, LogicalDevice* m_device, VkQueue transferQueue);

    VkSamplerAddressMode GetVkWrapMode(s32 wrapMode);
    VkFilter             GetVkFilterMode(s32 filterMode);

    void LoadTextureSamplers(tinygltf::Model& gltfModel);
    void LoadMaterials(tinygltf::Model& gltfModel);

    void SetupDescriptorSet(Node* node);
    void SetupNodeDescriptorSet(Node* node, DescriptorPoolGrowable* descriptorPool, DescriptorSetLayout* layout);
};
} // namespace Humongous
