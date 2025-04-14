#pragma once

#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool_growable.hpp"
#include "logical_device.hpp"
#include "model.hpp"
#include "singleton.hpp"

namespace Humongous
{

class ResourceManager : Singleton<ResourceManager>
{
private:
    struct ModelDescriptors;

public:
    static void Init(LogicalDevice* logicalDevice) { Get().Internal_Init(logicalDevice); }
    static void Shutdown() { Get().Internal_Shutdown(); }

    static std::shared_ptr<Model> LoadModel(std::string name) { return Get().Internal_LoadModel(name); };

    static const ModelDescriptors& GetModelDescriptors() { return Get().m_modelDescriptors; }

    // Material, MaterialBuffer, Node, RendererBuffer
    static std::vector<VkDescriptorSetLayout> GetLayoutVector()
    {
        return {Get().m_modelDescriptors.materialLayout->GetDescriptorSetLayout(),
                Get().m_modelDescriptors.materialBufferLayout->GetDescriptorSetLayout(),
                Get().m_modelDescriptors.nodeLayout->GetDescriptorSetLayout(), Get().m_modelDescriptors.debugLayout->GetDescriptorSetLayout()};
    }

    static n32 GetTotalModelBindingCount()
    {
        return Get().m_modelDescriptors.materialLayout->GetBindingCount() + Get().m_modelDescriptors.materialBufferLayout->GetBindingCount() +
               Get().m_modelDescriptors.nodeLayout->GetBindingCount() + Get().m_modelDescriptors.debugLayout->GetBindingCount();
    }

private:
    struct ModelDescriptors
    {
        std::unique_ptr<DescriptorPoolGrowable> imagePool;
        std::unique_ptr<DescriptorPoolGrowable> uniformPool;
        std::unique_ptr<DescriptorPoolGrowable> storagePool;
        std::unique_ptr<DescriptorPoolGrowable> debugPool;

        std::unique_ptr<DescriptorSetLayout> materialLayout;
        std::unique_ptr<DescriptorSetLayout> nodeLayout;
        std::unique_ptr<DescriptorSetLayout> materialBufferLayout;
        std::unique_ptr<DescriptorSetLayout> rendererBuffer;
        std::unique_ptr<DescriptorSetLayout> debugLayout;
    } m_modelDescriptors;

    struct WorldDescriptors
    {

    } m_worldDescritors;

    LogicalDevice* m_logicalDevice{nullptr};

    void Internal_Init(LogicalDevice* device);
    void InitDescriptors();
    void Internal_Shutdown();

    std::shared_ptr<Model> Internal_LoadModel(std::string name);
};
} // namespace Humongous
