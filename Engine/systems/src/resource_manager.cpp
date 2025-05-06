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
    m_modelDescriptors.nodeIdLayout.reset();
    m_modelDescriptors.nodeLayout.reset();
    m_modelDescriptors.materialDataLayout.reset();
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
    std::vector<vk::DescriptorType> t1 = {vk::DescriptorType::eCombinedImageSampler};
    std::vector<vk::DescriptorType> t2 = {vk::DescriptorType::eUniformBuffer};
    std::vector<vk::DescriptorType> t3 = {vk::DescriptorType::eStorageBuffer};

    m_descriptorPools.imagePool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t1);
    m_descriptorPools.uniformPool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t2);
    m_descriptorPools.storagePool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t3);
    m_descriptorPools.debugPool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t3);

    DescriptorSetLayout::Builder materialBufferBuilder{*m_logicalDevice};
    materialBufferBuilder.addBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment);
    m_modelDescriptors.materialDataLayout = materialBufferBuilder.Build();

    DescriptorSetLayout::Builder nodeIDBufferBuilder{*m_logicalDevice};
    nodeIDBufferBuilder.addBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex);
    m_modelDescriptors.nodeIdLayout = nodeIDBufferBuilder.Build();

    DescriptorSetLayout::Builder nodeBuilder{*m_logicalDevice};
    nodeBuilder.addBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex);
    m_modelDescriptors.nodeLayout = nodeBuilder.Build();

    DescriptorSetLayout::Builder materialBuilder{*m_logicalDevice};
    materialBuilder.addBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    materialBuilder.addBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    materialBuilder.addBinding(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    materialBuilder.addBinding(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    materialBuilder.addBinding(4, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    m_modelDescriptors.materialLayout = materialBuilder.Build();

    DescriptorSetLayout::Builder debugBuilder{*m_logicalDevice};
    debugBuilder.addBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex);
    m_modelDescriptors.debugLayout = debugBuilder.Build();

    DescriptorSetLayout::Builder builder{*m_logicalDevice};
    builder.addBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.addBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.addBinding(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.addBinding(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    m_skyboxLayout = builder.Build();
}

std::shared_ptr<Model> ResourceManager::Internal_LoadModel(const std::string& name)
{
    auto m = std::make_shared<Model>(m_logicalDevice, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::MODEL, name), 1.0f);
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
