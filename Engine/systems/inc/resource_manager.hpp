#pragma once

#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool_growable.hpp"
#include "audio_source.hpp"
#include "logical_device.hpp"
#include "model.hpp"
#include "model_instance.hpp"
#include "skybox.hpp"

#include <Eigen/Dense>

namespace Humongous
{

class ResourceManager : NonCopyable
{
private:
    struct ModelDescriptors;
    struct DescriptorPools;
    struct MaterialKey;

public:
    ResourceManager(const LogicalDevice& logicalDevice);
    ~ResourceManager();

    std::shared_ptr<ModelInstance> RequestModel(const std::string& name);
    n32                            RequestModelNodeMatriciesIndex(const n32& index);

    void FinalizeGPUData();

    void AddIndicesToModel(const std::vector<n32>& modelIndices, std::vector<Primitive*>& modelPrimitives);
    void AddVerticesToModel(const std::vector<Model::Vertex>& modelVertices, const std::vector<Mesh*>& modelMeshes);

    void UpdateNodeMatrices(const std::vector<Eigen::Matrix4f>& nodeMatrices, const n32& handle);

    void AddJointMatriciesToModel(const std::vector<Eigen::Matrix4f>& jointMatricies, const n32& handle);

    void UpdateJointMatrices(const std::vector<Eigen::Matrix4f>& jointMatricies, const n32& handle);

    void AddMorphTargetsToModel(const std::vector<f32>& morphTargets, const n32& handle);
    void UpdateMorphTargets(const std::vector<f32>& morphTargets, const n32& handle);

    std::shared_ptr<Model>         GetModel(const n32& index);
    std::shared_ptr<ModelInstance> GetModelInstance(const n32& index)
    {
        auto i = m_modelInstanceMap.at(index);
        return i;
    }

    std::shared_ptr<Skybox> LoadSkybox(const std::string& name);
    n32                     LoadAudioSource(const std::string& name);

    const ModelDescriptors& GetModelDescriptors() { return m_modelDescriptors; }
    const DescriptorPools&  GetDescriptorPools() { return m_descriptorPools; }

    vk::DescriptorSetLayout GetSkyboxDescriptorLayout() { return m_skyboxLayout->GetDescriptorSetLayout(); }
    vk::DescriptorSetLayout GetSkyboxCompDescriptorLayout() { return m_skyboxCompLayout->GetDescriptorSetLayout(); }

    void BindGlobalDescriptorSets(vk::CommandBuffer cmd, vk::PipelineLayout layout);

    // Bindless, node matricies, vertices, debug
    std::vector<vk::DescriptorSetLayout> GetLayoutVector()
    {
        return {m_bindlessLayout->GetDescriptorSetLayout(), m_modelDescriptors.vertices->GetDescriptorSetLayout(),
                m_modelDescriptors.debugLayout->GetDescriptorSetLayout()};
    }

    vk::DescriptorSet GetVertexDescriptor() { return m_modelDescriptors.vertexDescriptor; }

    n32 RequestTexture(class tinygltf::Image img, struct Texture::TexSamplerInfo sampler);
    n32 RequestMaterial(const Model::ShaderMaterial& mat);

    Buffer& GetModelIndexBuffer() { return *m_modelIndexBuffer; }
    n32&    GetModelHandleToIndexBufferStart(const n32& handle) { return m_modelHandleToIndexStart.at(handle); }
    n32&    GetModelHandleToMatrixStart(const n32& handle) { return m_modelHandleToMatrixStart.at(handle).first; }
    Buffer& GetModelVertexBuffer() { return *m_modelVertexBuffer; }

    std::unordered_map<n32, std::pair<n32, n32>> m_modelHandleToJointStart;
    std::unordered_map<n32, std::pair<n32, n32>> m_modelHandleToMorphStart;
    std::unordered_map<n32, std::pair<n32, n32>> m_modelHandleToMeshletStart;
    std::unordered_map<n32, std::pair<n32, n32>> m_modelHandleToMeshletIndexStart;
    std::unique_ptr<Buffer>                      m_meshletBuffer;
    std::unique_ptr<Buffer>                      m_meshletVertexBuffer;
    std::unique_ptr<Buffer>                      m_meshletPrimitiveBuffer;
    std::vector<Meshlet>                         m_meshlets;
    std::vector<n32>                             m_meshletVertices;
    std::vector<n8>                              m_meshletPrimitives;

    void AddMeshletsToModel(const std::vector<Meshlet>& meshlets, const std::vector<n32>& meshletVertices, const std::vector<n8>& meshletPrimitives,
                            const n32& handle);

private:
    struct DescriptorPools
    {
        std::unique_ptr<DescriptorPoolGrowable> imagePool;
        std::unique_ptr<DescriptorPoolGrowable> uniformPool;
        std::unique_ptr<DescriptorPoolGrowable> storageBufferPool;
        std::unique_ptr<DescriptorPoolGrowable> storageImagePool;
        std::unique_ptr<DescriptorPoolGrowable> debugPool;
    } m_descriptorPools;

    struct ModelDescriptors
    {
        vk::DescriptorSet                    vertexDescriptor;
        std::unique_ptr<DescriptorSetLayout> vertices;
        std::unique_ptr<DescriptorSetLayout> debugLayout;
        std::unique_ptr<DescriptorSetLayout> rendererBuffer;
    } m_modelDescriptors;

    std::unique_ptr<DescriptorSetLayout> m_skyboxLayout;
    std::unique_ptr<DescriptorSetLayout> m_skyboxCompLayout;

    const LogicalDevice& m_logicalDevice;

    void InitDescriptors();
    void InitializeInitials();

    std::unordered_map<n32, std::shared_ptr<Model>> m_modelMap;
    std::unordered_map<std::string, n32>            m_modelNameToHandle;

    std::unordered_map<n32, std::shared_ptr<ModelInstance>> m_modelInstanceMap;

    std::unique_ptr<Buffer>      m_modelIndexBuffer;
    std::unordered_map<n32, n32> m_modelHandleToIndexStart;
    std::unique_ptr<Buffer>      m_modelVertexBuffer;
    std::vector<n32>             m_modelIndicies;
    std::vector<Model::Vertex>   m_modelVertices;

    std::vector<Eigen::Matrix4f> m_modelJointMatricies;
    std::unique_ptr<Buffer>      m_modelJointMatriciesBuffer;

    std::vector<f32>        m_modelMorphTargets;
    std::unique_ptr<Buffer> m_modelMorphTargetsBuffer;

    std::unordered_map<n32, std::pair<n32, n32>> m_modelHandleToMatrixStart;
    std::vector<Eigen::Matrix4f>                 m_modelNodeMatricesFlat;
    std::unique_ptr<Buffer>                      m_modelNodeMatriciesBuffer;

    n32 m_nextModelID{0};
    n32 m_prevModelID{0};

    n32 m_nextInstanceID{0};

    n32 LoadModel(const std::string& name);

    std::unordered_map<n32, std::shared_ptr<AudioSourceComponent>> m_audioMap;
    n32                                                            m_nextaudioID{0};
    std::shared_ptr<AudioSourceComponent>                          GetAudioSource(const n32& index);

    struct TextureBinding
    {
        Texture* texture;
        n32      bindlessIndex;
    };

    std::vector<std::unique_ptr<Texture>>           m_textures;
    std::unordered_map<std::string, TextureBinding> m_textureMap;
    std::vector<vk::DescriptorImageInfo>            m_bindlessImageInfos;
    vk::DescriptorSet                               m_bindlessSet;
    std::unique_ptr<DescriptorSetLayout>            m_bindlessLayout;
    std::unique_ptr<DescriptorPoolGrowable>         m_bindlessTexturePool;
    uint32_t                                        m_nextBindlessIndex = 0;

    struct MaterialBinding
    {
        Model::ShaderMaterial material;
        n32                   bindlessIndex;
    };

    std::vector<MaterialBinding>         m_materials;
    std::unordered_map<std::string, n32> m_materialMap;
    std::unique_ptr<Buffer>              m_materialDataBuffer;
};
} // namespace Humongous
