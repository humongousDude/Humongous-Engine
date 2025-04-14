#include "resource_manager.hpp"
#include "asset_manager.hpp"
#include "logger.hpp"

namespace Humongous
{

void ResourceManager::Internal_Init(LogicalDevice* device)
{
    HGINFO("Initializing Resource manager...");
    m_logicalDevice = device;
    InitDescriptors();
    HGINFO("Resource manager initialized");
}

void ResourceManager::Internal_Shutdown()
{
    HGINFO("Shutting down resource manager...");

    m_modelDescriptors.materialLayout.reset();
    m_modelDescriptors.materialBufferLayout.reset();
    m_modelDescriptors.nodeLayout.reset();
    m_modelDescriptors.debugLayout.reset();

    m_modelDescriptors.imagePool.reset();
    m_modelDescriptors.uniformPool.reset();
    m_modelDescriptors.storagePool.reset();
    m_modelDescriptors.debugPool.reset();

    HGINFO("Resource manager shutdown");
}

void ResourceManager::InitDescriptors()
{
    std::vector<VkDescriptorType> t1 = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
    std::vector<VkDescriptorType> t2 = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};
    std::vector<VkDescriptorType> t3 = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};

    m_modelDescriptors.imagePool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, t1);
    m_modelDescriptors.uniformPool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, t2);
    m_modelDescriptors.storagePool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, t3);
    m_modelDescriptors.debugPool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, t3);

    DescriptorSetLayout::Builder nodeBuilder{*m_logicalDevice};
    nodeBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    m_modelDescriptors.nodeLayout = nodeBuilder.Build();

    DescriptorSetLayout::Builder materialBufferBuilder{*m_logicalDevice};
    materialBufferBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_modelDescriptors.materialBufferLayout = materialBufferBuilder.Build();

    DescriptorSetLayout::Builder materialBuilder{*m_logicalDevice};
    materialBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_modelDescriptors.materialLayout = materialBuilder.Build();

    DescriptorSetLayout::Builder debugBuilder{*m_logicalDevice};
    debugBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    m_modelDescriptors.debugLayout = debugBuilder.Build();
}

std::shared_ptr<Model> ResourceManager::Internal_LoadModel(std::string name)
{
    auto m = std::make_shared<Model>(m_logicalDevice, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::MODEL, name), 0.1f);
    m->Init(m_modelDescriptors.materialLayout.get(), m_modelDescriptors.nodeLayout.get(), m_modelDescriptors.materialBufferLayout.get(),
            m_modelDescriptors.imagePool.get(), m_modelDescriptors.uniformPool.get(), m_modelDescriptors.storagePool.get());
    return m;
}

} // namespace Humongous
