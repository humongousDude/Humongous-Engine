#pragma once

// TODO: Cleanup

// based of off Sascha Willems tinyGltf vulkan example

#include "abstractions/descriptor_layout.hpp"

#include "abstractions/buffer.hpp"
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
    Node*       m_owner;
    u32         m_firstIndex;
    u32         m_indexCount;
    u32         m_vertexCount;
    Material&   m_material;
    bool        m_hasIndices;
    BoundingBox m_bb;
    Primitive(u32 firstIndex, u32 indexCount, u32 vertexCount, Material& material);
    void SetBoundingBox(glm::vec3 min, glm::vec3 max);
};

struct Mesh
{
    Mesh(LogicalDevice* m_device, glm::mat4 matrix);
    ~Mesh();
    LogicalDevice*          m_device;
    std::vector<Primitive*> m_primitives;
    BoundingBox             m_bb;
    BoundingBox             m_aabb;
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

    void SetBoundingBox(glm::vec3 min, glm::vec3 max);
};

class Model
{
public:
    struct alignas(16) PushConstantData // Align to 16 bytes for mat4
    {
        glm::mat4       model{1.f};    // 16 bytes (4x4 matrix)
        VkDeviceAddress vertexAddress; // 8 bytes (assuming 64-bit)
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

    glm::mat4 GetAABB() const { return m_aabb; }

    struct Dimensions
    {
        glm::vec3 min = glm::vec3(FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX);
    };

    Dimensions GetDimensions() const { return m_dimensions; }

private:
    Buffer m_vertices;
    Buffer m_indices;

    LogicalDevice* m_device;

    glm::mat4 m_aabb;

    std::vector<Node*> m_nodes;
    std::vector<Node*> m_linearNodes;

    Texture                                          m_emptyTexture;
    std::vector<Texture>                             m_textures;
    std::vector<Texture::TexSamplerInfo>             m_textureSamplers;
    std::vector<Material>                            m_materials;
    std::unordered_map<u32, std::vector<Primitive*>> m_materialBatches;

    VkDescriptorSet descriptorSetMaterials{VK_NULL_HANDLE};
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

    Dimensions m_dimensions;

    struct LoaderInfo
    {
        u32*    indexBuffer;
        Vertex* vertexBuffer;
        size_t  indexPos = 0;
        size_t  vertexPos = 0;
    };

    bool m_initialized{false};
    // TODO: maybe move the shader material buffer and this out?
    // maybe only write at draw time?
    void CreateMaterialBuffer();
    void UpdateShaderMaterialBuffer(Node* node);
    void UpdateUBO(Node* node, glm::mat4 matrix);
    void UpdateMaterialBatches(Node* node);

    void Destroy(VkDevice m_device);
    void LoadNode(Node* parent, const tinygltf::Node& node, u32 nodeIndex, const tinygltf::Model& model, LoaderInfo& loaderInfo, float globalscale);
    void GetNodeProps(const tinygltf::Node& node, const tinygltf::Model& model, size_t& vertexCount, size_t& indexCount);
    void LoadTextures(tinygltf::Model& gltfModel, LogicalDevice* m_device, VkQueue transferQueue);
    VkSamplerAddressMode GetVkWrapMode(i32 wrapMode);
    VkFilter             GetVkFilterMode(i32 filterMode);
    void                 LoadTextureSamplers(tinygltf::Model& gltfModel);
    void                 LoadMaterials(tinygltf::Model& gltfModel);
    void                 LoadFromFile(std::string filename, LogicalDevice* device, VkQueue transferQueue, float scale = 1.0f);
    void                 DrawNode(Node* node, VkCommandBuffer commandBuffer, VkPipelineLayout& pipelineLayout);
    void                 CalculateBoundingBox(Node* node, Node* parent);
    void                 GetSceneDimensions();
    Node*                FindNode(Node* parent, u32 index);
    Node*                NodeFromIndex(u32 index);
    void                 SetupDescriptorSet(Node* node);
    void                 SetupNodeDescriptorSet(Node* node, DescriptorPoolGrowable* descriptorPool, DescriptorSetLayout* layout);
};
} // namespace Humongous
