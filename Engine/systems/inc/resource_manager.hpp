#pragma once

#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool_growable.hpp"
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

    static std::shared_ptr<Model>  LoadModel(const std::string& name) { return Get().Internal_LoadModel(name); };
    static std::shared_ptr<Skybox> LoadSkybox(const std::string& name) { return Get().Internal_LoadSkybox(name); }

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
        std::unique_ptr<DescriptorPoolGrowable> storagePool;
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

    struct WorldDescriptors
    {

    } m_worldDescritors;

    LogicalDevice* m_logicalDevice{nullptr};

    void Internal_Init(LogicalDevice* device);
    void InitDescriptors();
    void Internal_Shutdown();

    std::shared_ptr<Model>  Internal_LoadModel(const std::string& name);
    std::shared_ptr<Skybox> Internal_LoadSkybox(const std::string& name);
};
} // namespace Humongous
