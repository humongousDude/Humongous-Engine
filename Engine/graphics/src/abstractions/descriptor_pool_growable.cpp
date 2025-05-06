// Taken from vkguide.dev
#include "logger.hpp"
#include <abstractions/descriptor_pool_growable.hpp>

// TODO: Change this to use vulkan.hpp
namespace Humongous
{
DescriptorPoolGrowable::DescriptorPoolGrowable(LogicalDevice& logicalDevice, n32 maxSets, vk::DescriptorPoolCreateFlags m_poolFlags,
                                               std::vector<vk::DescriptorType>& poolTypes)
    : m_logicalDevice{logicalDevice}
{
    m_poolTypes.clear();

    for(auto t: poolTypes) { m_poolTypes.push_back(t); }

    vk::DescriptorPool newPool = CreatePool(logicalDevice, maxSets, poolTypes);

    m_setsPerPool = maxSets * 1.5;

    m_readyPools.push_back(newPool);
}

DescriptorPoolGrowable::~DescriptorPoolGrowable()
{
    for(auto pool: m_readyPools) { m_logicalDevice.GetVkDevice().destroyDescriptorPool(pool, nullptr); }
    for(auto pool: m_fullPools) { m_logicalDevice.GetVkDevice().destroyDescriptorPool(pool, nullptr); }
    m_readyPools.clear();
    m_fullPools.clear();
}

bool DescriptorPoolGrowable::AllocateDescriptor(const vk::DescriptorSetLayout descriptorSetLayout, vk::DescriptorSet& descriptor)
{
    // Get or create a pool to allocate from
    vk::DescriptorPool poolToUse = GetPool(m_logicalDevice);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = vk::StructureType::eDescriptorSetAllocateInfo;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    vk::Result result = m_logicalDevice.GetVkDevice().allocateDescriptorSets(&allocInfo, &descriptor);

    if(result == vk::Result::eErrorOutOfPoolMemory)
    {
        m_fullPools.push_back(poolToUse);

        poolToUse = GetPool(m_logicalDevice);
        allocInfo.descriptorPool = poolToUse;

        vk::Result result = m_logicalDevice.GetVkDevice().allocateDescriptorSets(&allocInfo, &descriptor);

        if(result != vk::Result::eSuccess)
        {
            HGERROR("Completely failed to allocate a descriptor set, failing");
            HGERROR("Error Code: %d", result);
            return false;
        }
    }

    m_readyPools.push_back(poolToUse);

    return true;
}

vk::DescriptorSet DescriptorPoolGrowable::AllocateDescriptor(const vk::DescriptorSetLayout descriptorSetLayout)
{
    vk::DescriptorSet descriptor;
    AllocateDescriptor(descriptorSetLayout, descriptor);
    return descriptor;
}

void DescriptorPoolGrowable::ResetPools()
{
    for(int i = 0; i < m_fullPools.size(); i++) { m_logicalDevice.GetVkDevice().resetDescriptorPool(m_fullPools[i]); }
    m_fullPools.clear();
    for(int i = 0; i < m_readyPools.size(); i++) { m_logicalDevice.GetVkDevice().resetDescriptorPool(m_readyPools[i]); }
    m_readyPools.clear();
}

vk::DescriptorPool DescriptorPoolGrowable::GetPool(LogicalDevice& logicalDevice)
{
    vk::DescriptorPool newPool;
    if(m_readyPools.size() != 0)
    {
        newPool = m_readyPools.back();
        m_readyPools.pop_back();
    }
    else
    {
        newPool = CreatePool(logicalDevice, m_setsPerPool, m_poolTypes);

        m_setsPerPool = m_setsPerPool * 1.5;

        if(m_setsPerPool > 4092) { m_setsPerPool = 4092; }
    }

    return newPool;
}

vk::DescriptorPool DescriptorPoolGrowable::CreatePool(LogicalDevice& logicalDevice, n32 setCount, std::vector<vk::DescriptorType> poolTypes) const
{
    std::vector<vk::DescriptorPoolSize> poolSizes;
    for(vk::DescriptorType type: poolTypes) { poolSizes.push_back({type, setCount}); }

    vk::DescriptorPoolCreateInfo info{};
    info.sType = vk::StructureType::eDescriptorPoolCreateInfo;
    info.maxSets = setCount;
    info.poolSizeCount = static_cast<n32>(poolSizes.size());
    info.pPoolSizes = poolSizes.data();

    vk::DescriptorPool newPool;
    if(logicalDevice.GetVkDevice().createDescriptorPool(&info, nullptr, &newPool) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create descriptor pool");
    }

    return newPool;
}
} // namespace Humongous
