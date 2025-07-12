#pragma once

#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool_growable.hpp"
#include "audio_source.hpp"
#include "logical_device.hpp"
#include "model.hpp"
#include "model_instance.hpp"
#include "singleton.hpp"
#include "skybox.hpp"

namespace Humongous
{

class ResourceManager : public Singleton<ResourceManager>
{
private:
    struct ModelDescriptors;
    struct DescriptorPools;
    struct MaterialKey;

public:
    static void Init(LogicalDevice* logicalDevice) { Get().Internal_Init(logicalDevice); }
    static void Shutdown() { Get().Internal_Shutdown(); }

    static std::shared_ptr<ModelInstance> RequestModel(const std::string& name) { return Get().Internal_RequestModel(name); };
    static n32 RequestModelNodeMatriciesIndex(const n32& index) { return Get().Internal_RequestModelNodeMatriciesIndex(index); };

    static void FinalizeGPUData() { Get().Internal_FinalizeGPUData(); }

    static void AddIndicesToModel(const std::vector<n32>& modelIndices, std::vector<Primitive*>& modelPrimitives)
    {
        Get().Internal_AddIndicesToModel(modelIndices, modelPrimitives);
    }
    static void AddVerticesToModel(const std::vector<Model::Vertex>& modelVertices, const std::vector<Mesh*>& modelMeshes)
    {
        Get().Internal_AddVerticesToModel(modelVertices, modelMeshes);
    }

    static void UpdateNodeMatrices(const std::vector<glm::mat4>& nodeMatrices, const n32& handle)
    {
        Get().Internal_UpdateNodeMatrices(nodeMatrices, handle);
    }

    static void AddJointMatriciesToModel(const std::vector<glm::mat4>& jointMatricies, const n32& handle)
    {
        Get().Internal_AddJointMatriciesToModel(jointMatricies, handle);
    }

    static void UpdateJointMatrices(const std::vector<glm::mat4>& jointMatricies, const n32& handle)
    {
        Get().Internal_UpdateJointMatrices(jointMatricies, handle);
    }

    static void AddMorphTargetsToModel(const std::vector<f32>& morphTargets, const n32& handle)
    {
        Get().Internal_AddMorphTargetsToModel(morphTargets, handle);
    }
    static void UpdateMorphTargets(const std::vector<f32>& morphTargets, const n32& handle)
    {
        Get().Internal_UpdateMorphTargets(morphTargets, handle);
    }

    static std::shared_ptr<Model>         GetModel(const n32& index) { return Get().Internal_GetModel(index); }
    static std::shared_ptr<ModelInstance> GetModelInstance(const n32& index)
    {
        auto i = Get().m_modelInstanceMap.at(index);
        return i;
    }

    static std::shared_ptr<Skybox> LoadSkybox(const std::string& name) { return Get().Internal_LoadSkybox(name); }
    n32                            LoadAudioSource(const std::string& name) { return Get().Internal_LoadAudioSource(name); };

    static const ModelDescriptors& GetModelDescriptors() { return Get().m_modelDescriptors; }
    static const DescriptorPools&  GetDescriptorPools() { return Get().m_descriptorPools; }

    static vk::DescriptorSetLayout GetSkyboxDescriptorLayout() { return Get().m_skyboxLayout->GetDescriptorSetLayout(); }
    static vk::DescriptorSetLayout GetSkyboxCompDescriptorLayout() { return Get().m_skyboxCompLayout->GetDescriptorSetLayout(); }

    static void BindGlobalDescriptorSets(vk::CommandBuffer cmd, vk::PipelineLayout layout) { Get().Internal_BindGlobalDescriptorSets(cmd, layout); }

    // Bindless, node matricies, vertices, debug
    static std::vector<vk::DescriptorSetLayout> GetLayoutVector()
    {
        return {Get().m_bindlessLayout->GetDescriptorSetLayout(), Get().m_modelDescriptors.vertices->GetDescriptorSetLayout(),
                Get().m_modelDescriptors.debugLayout->GetDescriptorSetLayout()};
    }

    static vk::DescriptorSet GetVertexDescriptor() { return Get().m_modelDescriptors.vertexDescriptor; }

    static n32 RequestTexture(class tinygltf::Image img, struct Texture::TexSamplerInfo sampler)
    {
        return Get().Internal_RequestTexture(img, sampler);
    };

    static n32 RequestMaterial(const Model::ShaderMaterial& mat) { return Get().Internal_RequestMaterial(mat); }

    static Buffer& GetModelIndexBuffer() { return *Get().m_modelIndexBuffer; }
    static n32&    GetModelHandleToIndexBufferStart(const n32& handle) { return Get().m_modelHandleToIndexStart.at(handle); }
    static n32&    GetModelHandleToMatrixStart(const n32& handle) { return Get().m_modelHandleToMatrixStart.at(handle).first; }
    static Buffer& GetModelVertexBuffer() { return *Get().m_modelVertexBuffer; }

    std::unordered_map<n32, std::pair<n32, n32>> m_modelHandleToJointStart;
    std::unordered_map<n32, std::pair<n32, n32>> m_modelHandleToMorphStart;

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

    LogicalDevice* m_logicalDevice{nullptr};

    void Internal_Init(LogicalDevice* device);
    void InitDescriptors();
    void InitializeInitials();
    void Internal_Shutdown();

    void Internal_FinalizeGPUData();

    std::unordered_map<n32, std::shared_ptr<Model>> m_modelMap;
    std::unordered_map<std::string, n32>            m_modelNameToHandle;

    std::unordered_map<n32, std::shared_ptr<ModelInstance>> m_modelInstanceMap;

    std::unique_ptr<Buffer>      m_modelIndexBuffer;
    std::unordered_map<n32, n32> m_modelHandleToIndexStart;
    std::unique_ptr<Buffer>      m_modelVertexBuffer;
    std::vector<n32>             m_modelIndicies;
    std::vector<Model::Vertex>   m_modelVertices;

    std::vector<glm::mat4>  m_modelJointMatricies;
    std::unique_ptr<Buffer> m_modelJointMatriciesBuffer;

    std::vector<f32>        m_modelMorphTargets;
    std::unique_ptr<Buffer> m_modelMorphTargetsBuffer;

    std::unordered_map<n32, std::pair<n32, n32>> m_modelHandleToMatrixStart;
    std::vector<glm::mat4>                       m_modelNodeMatricesFlat;
    std::unique_ptr<Buffer>                      m_modelNodeMatriciesBuffer;

    n32 m_nextModelID{0};
    n32 m_prevModelID{0};

    n32 m_nextInstanceID{0};

    n32                            LoadModel(const std::string& name);
    std::shared_ptr<ModelInstance> Internal_RequestModel(const std::string& name);
    n32                            Internal_RequestModelNodeMatriciesIndex(const n32& index);
    std::shared_ptr<Model>         Internal_GetModel(const n32& index);

    void Internal_AddIndicesToModel(const std::vector<n32>& modelIndices, std::vector<Primitive*>& modelPrimitives);
    void Internal_AddVerticesToModel(const std::vector<Model::Vertex>& modelVertices, const std::vector<Mesh*>& modelMeshes);

    void Internal_UpdateNodeMatrices(const std::vector<glm::mat4>& nodeMatrices, const n32& handle);
    void Internal_AddJointMatriciesToModel(const std::vector<glm::mat4>& jointMatricies, const n32& handle);
    void Internal_UpdateJointMatrices(const std::vector<glm::mat4>& jointMatricies, const n32& handle);

    void Internal_AddMorphTargetsToModel(const std::vector<f32>& morphTargets, const n32& handle);
    void Internal_UpdateMorphTargets(const std::vector<f32>& morphTargets, const n32& handle);

    std::unordered_map<n32, std::shared_ptr<AudioSourceComponent>> m_audioMap;
    n32                                                            m_nextaudioID{0};
    n32                                                            Internal_LoadAudioSource(const std::string& name);
    std::shared_ptr<AudioSourceComponent>                          Internal_GetAudioSource(const n32& index);

    struct TextureBinding
    {
        Texture texture;
        n32     bindlessIndex;
    };

    std::unordered_map<std::string, TextureBinding> m_textureMap;
    std::vector<vk::DescriptorImageInfo>            m_bindlessImageInfos;
    vk::DescriptorSet                               m_bindlessSet;
    std::unique_ptr<DescriptorSetLayout>            m_bindlessLayout;
    std::unique_ptr<DescriptorPoolGrowable>         m_bindlessTexturePool;
    uint32_t                                        m_nextBindlessIndex = 0;
    void                                            Internal_BindGlobalDescriptorSets(vk::CommandBuffer cmd, vk::PipelineLayout layout);

    n32 Internal_RequestTexture(class tinygltf::Image img, struct Texture::TexSamplerInfo sampler);

    struct MaterialBinding
    {
        Model::ShaderMaterial material;
        n32                   bindlessIndex;
    };

    std::vector<MaterialBinding>         m_materials;
    std::unordered_map<std::string, n32> m_materialMap;
    std::unique_ptr<Buffer>              m_materialDataBuffer;

    n32 Internal_RequestMaterial(const Model::ShaderMaterial& mat);

    std::shared_ptr<Skybox> Internal_LoadSkybox(const std::string& name);
};
} // namespace Humongous
