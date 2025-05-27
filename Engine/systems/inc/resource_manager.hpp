#pragma once

#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool_growable.hpp"
#include "audio_source.hpp"
#include "logical_device.hpp"
#include "model.hpp"
#include "singleton.hpp"
#include "skybox.hpp"

namespace Humongous
{

class ResourceManager : Singleton<ResourceManager>
{
private:
    struct ModelDescriptors;
    struct DescriptorPools;
    struct MaterialKey;

public:
    static void Init(LogicalDevice* logicalDevice) { Get().Internal_Init(logicalDevice); }
    static void Shutdown() { Get().Internal_Shutdown(); }

    static n32                    LoadModel(const std::string& name) { return Get().Internal_LoadModel(name); };
    static std::shared_ptr<Model> GetModel(const n32& index) { return Get().Internal_GetModel(index); }

    static std::shared_ptr<Skybox> LoadSkybox(const std::string& name) { return Get().Internal_LoadSkybox(name); }
    n32                            LoadAudioSource(const std::string& name) { return Get().Internal_LoadAudioSource(name); };

    static const ModelDescriptors& GetModelDescriptors() { return Get().m_modelDescriptors; }
    static const DescriptorPools&  GetDescriptorPools() { return Get().m_descriptorPools; }

    static vk::DescriptorSetLayout GetSkyboxDescriptorLayout() { return Get().m_skyboxLayout->GetDescriptorSetLayout(); }

    static void BindGlobalDescriptorSets(vk::CommandBuffer cmd, vk::PipelineLayout layout) { Get().Internal_BindGlobalDescriptorSets(cmd, layout); }

    // Material Textures, Material Data, Node, RendererBuffer
    static std::vector<vk::DescriptorSetLayout> GetLayoutVector()
    {
        return {Get().m_bindlessLayout->GetDescriptorSetLayout(), Get().m_modelDescriptors.nodeLayout->GetDescriptorSetLayout(),
                Get().m_modelDescriptors.debugLayout->GetDescriptorSetLayout()};
    }

    static n32 GetTotalModelBindingCount()
    {
        return Get().m_modelDescriptors.nodeLayout->GetBindingCount() + Get().m_modelDescriptors.debugLayout->GetBindingCount();
    }

    static n32 RequestTexture(class tinygltf::Image img, struct Texture::TexSamplerInfo sampler)
    {
        return Get().Internal_RequestTexture(img, sampler);
    };

    static n32 RequestMaterial(const Model::ShaderMaterial& mat) { return Get().Internal_RequestMaterial(mat); }

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
        std::unique_ptr<DescriptorSetLayout> nodeLayout;
        std::unique_ptr<DescriptorSetLayout> debugLayout;
        std::unique_ptr<DescriptorSetLayout> rendererBuffer;
    } m_modelDescriptors;

    std::unique_ptr<DescriptorSetLayout> m_skyboxLayout;

    LogicalDevice* m_logicalDevice{nullptr};

    void Internal_Init(LogicalDevice* device);
    void InitDescriptors();
    void Internal_Shutdown();

    std::unordered_map<n32, std::shared_ptr<Model>> m_modelMap;
    n32                                             m_nextModelID{0};
    n32                                             Internal_LoadModel(const std::string& name);
    std::shared_ptr<Model>                          Internal_GetModel(const n32& index);

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
