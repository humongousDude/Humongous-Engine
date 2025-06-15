#pragma once

// TODO: Cleanup

// based on Sascha Willems' tinyGltf vulkan example

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
    Node*     owner;
    n32       firstIndex;
    n32       localVertexStart;
    n32       vertexOffset;
    n32       indexCount;
    n32       vertexCount;
    Material& material;
    bool      hasIndices;
    Primitive(n32 firstIndex, n32 indexCount, n32 vertexCount, n32 localVertexStart, Material& material)
        : firstIndex(firstIndex), indexCount(indexCount), vertexCount(vertexCount), localVertexStart(localVertexStart),
          vertexOffset(0) // we’ll fill this in _after_ we know the global base
          ,
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
};

class Model
{
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
        glm::vec4 position;   // 12 bytes (aligned to 16 bytes)
        glm::vec4 normal;     // 12 bytes (aligned to 16 bytes)
        glm::vec4 tangent;    // 12 bytes (aligned to 16 bytes)
        glm::vec4 bitTangent; // 12 bytes (aligned to 16 bytes)
        glm::vec4 uv0;        // 8 bytes
        glm::vec4 uv1;        // 8 bytes
        glm::vec4 color;      // 16 bytes

        bool operator==(const Vertex& other) const
        {
            return position == other.position && normal == other.normal && uv0 == other.uv0 && uv1 == other.uv1 && color == other.color;
        }
    };

    Model(LogicalDevice* device, const std::string& modelPath, float scale);
    ~Model();

    std::vector<n32>& GetIndices() { return m_indices; }

    void Init();

    struct Dimensions
    {
        glm::vec3 min = glm::vec3(FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX);
    };

    BoundingBox GetLocalBoundingBox() { return m_localAABB; }

    std::vector<glm::mat4> GetMatrixVector();

    std::unordered_map<n32, std::vector<Primitive*>> m_materialBatches;

private:
    std::vector<n32>    m_indices;
    std::vector<Vertex> m_vertices;

    LogicalDevice* m_logicalDevice;

    std::vector<Node*> m_nodes;
    std::vector<Node*> m_linearNodes;

    BoundingBox m_localAABB{};

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

    BoundingBox CalculateModelAABB(const std::vector<Node*>& rootNodes, const std::vector<Model::Vertex>& vertexBuffer,
                                   const std::vector<n32>& indexBuffer);

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

    vk::SamplerAddressMode GetVkWrapMode(s32 wrapMode);
    vk::Filter             GetVkFilterMode(s32 filterMode);

    void LoadTextureSamplers(tinygltf::Model& gltfModel);
    void LoadMaterials(tinygltf::Model& gltfModel);
};
} // namespace Humongous
