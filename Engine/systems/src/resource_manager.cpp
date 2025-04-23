#include "resource_manager.hpp"
#include "asset_manager.hpp"
#include "logger.hpp"
#include "skybox.hpp"

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
    m_modelDescriptors.materialDataLayout.reset();
    m_modelDescriptors.nodeLayout.reset();
    m_modelDescriptors.debugLayout.reset();

    m_descriptorPools.imagePool.reset();
    m_descriptorPools.uniformPool.reset();
    m_descriptorPools.storagePool.reset();
    m_descriptorPools.debugPool.reset();

    m_skyboxLayout.reset();

    HGINFO("Resource manager shutdown");
}

void ResourceManager::InitDescriptors()
{
    std::vector<VkDescriptorType> t1 = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
    std::vector<VkDescriptorType> t2 = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};
    std::vector<VkDescriptorType> t3 = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};

    m_descriptorPools.imagePool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, t1);
    m_descriptorPools.uniformPool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, t2);
    m_descriptorPools.storagePool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, t3);
    m_descriptorPools.debugPool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, t3);

    DescriptorSetLayout::Builder nodeBuilder{*m_logicalDevice};
    nodeBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    m_modelDescriptors.nodeLayout = nodeBuilder.Build();

    DescriptorSetLayout::Builder materialBufferBuilder{*m_logicalDevice};
    materialBufferBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_modelDescriptors.materialDataLayout = materialBufferBuilder.Build();

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

    DescriptorSetLayout::Builder builder{*m_logicalDevice};
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_skyboxLayout = builder.Build();
}

std::shared_ptr<Model> ResourceManager::Internal_LoadModel(const std::string& name)
{
    auto m = std::make_shared<Model>(m_logicalDevice, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::MODEL, name), 0.1f);
    m->Init(m_modelDescriptors.materialLayout.get(), m_modelDescriptors.nodeLayout.get(), m_modelDescriptors.materialDataLayout.get(),
            m_descriptorPools.imagePool.get(), m_descriptorPools.uniformPool.get(), m_descriptorPools.storagePool.get());
    return m;
}

std::shared_ptr<Skybox> ResourceManager::Internal_LoadSkybox(const std::string& name)
{
    SkyboxCreateInfo info{m_logicalDevice, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::TEXTURE, name), *m_skyboxLayout,
                          *m_descriptorPools.imagePool, *m_descriptorPools.uniformPool};
    auto             s = std::make_shared<Skybox>(info);
    return s;
}

} // namespace Humongous
