#pragma once

// based of off Sascha Willems tinyGltf vulkan example

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
    uint32_t    firstIndex;
    uint32_t    indexCount;
    uint32_t    vertexCount;
    Material&   material;
    bool        hasIndices;
    BoundingBox bb;
    Primitive(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, Material& material);
    void SetBoundingBox(glm::vec3 min, glm::vec3 max);
};

struct Mesh
{
    Mesh(LogicalDevice* device, glm::mat4 matrix);
    ~Mesh();
    LogicalDevice*          device;
    std::vector<Primitive*> primitives;
    BoundingBox             bb;
    BoundingBox             aabb;
    struct UniformBuffer
    {
        Buffer                 uniformBuffer;
        VkDescriptorBufferInfo descriptorInfo;
        VkDescriptorSet        descriptorSet = VK_NULL_HANDLE;
    } uniformBuffer;

    struct UniformBlock
    {
        glm::mat4 matrix{1.f};
    } uniformBlock;

    void SetBoundingBox(glm::vec3 min, glm::vec3 max);
};

class Model
{
public:
    struct PushConstantData
    {
        alignas(16) glm::mat4 model{1.f};
        VkDeviceAddress vertexAddress{0};
    };
    struct Vertex
    {
        alignas(16) glm::vec3 position;
        alignas(16) glm::vec3 normal;
        alignas(8) glm::vec2 uv0;
        alignas(8) glm::vec2 uv1;
    };

    Model(LogicalDevice* device, const std::string& modelPath, float scale);
    ~Model();

    Buffer& GetVertexBuffer() { return vertices; }

    void Init(DescriptorSetLayout* materialLayout, DescriptorSetLayout* nodeLayout, DescriptorSetLayout* materialBufferLayout,
              DescriptorPoolGrowable* imagePool, DescriptorPoolGrowable* uniformPool, DescriptorPoolGrowable* storagePool);

    void Draw(VkCommandBuffer commandBuffer, VkPipelineLayout& pipelineLayout);

private:
    Buffer vertices;
    Buffer indices;

    LogicalDevice* device;

    glm::mat4 aabb;

    std::vector<Node*> nodes;
    std::vector<Node*> linearNodes;

    Texture                              emptyTexture;
    std::vector<Texture>                 textures;
    std::vector<Texture::TexSamplerInfo> textureSamplers;
    std::vector<Material>                materials;

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
    Buffer shaderMaterialBuffer{};

    struct Dimensions
    {
        glm::vec3 min = glm::vec3(FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX);
    } dimensions;

    struct LoaderInfo
    {
        uint32_t* indexBuffer;
        Vertex*   vertexBuffer;
        size_t    indexPos = 0;
        size_t    vertexPos = 0;
    };

    bool initialized{false};
    // TODO: maybe move the shader material buffer and this out?
    // maybe only write at draw time?
    void CreateMaterialBuffer();
    void UpdateShaderMaterialBuffer(Node* node);
    void UpdateUBO(Node* node, glm::mat4 matrix);

    void Destroy(VkDevice device);
    void LoadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, LoaderInfo& loaderInfo,
                  float globalscale);
    void GetNodeProps(const tinygltf::Node& node, const tinygltf::Model& model, size_t& vertexCount, size_t& indexCount);
    void LoadTextures(tinygltf::Model& gltfModel, LogicalDevice* device, VkQueue transferQueue);
    VkSamplerAddressMode GetVkWrapMode(int32_t wrapMode);
    VkFilter             GetVkFilterMode(int32_t filterMode);
    void                 LoadTextureSamplers(tinygltf::Model& gltfModel);
    void                 LoadMaterials(tinygltf::Model& gltfModel);
    void                 LoadFromFile(std::string filename, LogicalDevice* device, VkQueue transferQueue, float scale = 1.0f);
    void                 DrawNode(Node* node, VkCommandBuffer commandBuffer, VkPipelineLayout& pipelineLayout);
    void                 CalculateBoundingBox(Node* node, Node* parent);
    void                 GetSceneDimensions();
    Node*                FindNode(Node* parent, uint32_t index);
    Node*                NodeFromIndex(uint32_t index);
    void                 SetupDescriptorSet(Node* node);
    void                 SetupNodeDescriptorSet(Node* node, DescriptorPoolGrowable* descriptorPool, DescriptorSetLayout* layout);
};
} // namespace Humongous
