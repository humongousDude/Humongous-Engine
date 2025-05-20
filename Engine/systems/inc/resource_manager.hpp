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

    // Material Textures, Material Data, Node, RendererBuffer
    static std::vector<vk::DescriptorSetLayout> GetLayoutVector()
    {
        return {Get().m_modelDescriptors.materialDataLayout->GetDescriptorSetLayout(),
                Get().m_modelDescriptors.nodeIdLayout->GetDescriptorSetLayout(), Get().m_modelDescriptors.nodeLayout->GetDescriptorSetLayout(),
                Get().m_modelDescriptors.materialLayout->GetDescriptorSetLayout(), Get().m_modelDescriptors.debugLayout->GetDescriptorSetLayout()};
    }

    static n32 GetTotalModelBindingCount()
    {
        return Get().m_modelDescriptors.materialLayout->GetBindingCount() + Get().m_modelDescriptors.materialDataLayout->GetBindingCount() +
               Get().m_modelDescriptors.nodeIdLayout->GetBindingCount() + Get().m_modelDescriptors.nodeLayout->GetBindingCount() +
               Get().m_modelDescriptors.debugLayout->GetBindingCount();
    }

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
        std::unique_ptr<DescriptorSetLayout> materialDataLayout;
        std::unique_ptr<DescriptorSetLayout> nodeIdLayout;
        std::unique_ptr<DescriptorSetLayout> materialLayout;
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

    std::shared_ptr<Skybox> Internal_LoadSkybox(const std::string& name);
};
} // namespace Humongous
