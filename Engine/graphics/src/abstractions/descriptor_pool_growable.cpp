// Taken from vkguide.dev
#include "logger.hpp"
#include <abstractions/descriptor_pool_growable.hpp>

// TODO: Change this to use vulkan.hpp
namespace Humongous
{
DescriptorPoolGrowable::DescriptorPoolGrowable(const ILogicalDevice& logicalDevice, u32 maxSets, vk::DescriptorPoolCreateFlags poolFlags,
                                               std::vector<vk::DescriptorType>& poolTypes)
    : m_logicalDevice{logicalDevice}
{
    m_poolTypes.clear();

    for(auto t: poolTypes) { m_poolTypes.push_back(t); }

    vk::DescriptorPool newPool = CreatePool(logicalDevice, maxSets, poolTypes, poolFlags);

    m_setsPerPool = maxSets * 1.5;

    m_readyPools.push_back(newPool);
}

DescriptorPoolGrowable::~DescriptorPoolGrowable()
{
    for(auto pool: m_readyPools) { m_logicalDevice.DestroyDescriptorPool(pool); }
    for(auto pool: m_fullPools) { m_logicalDevice.DestroyDescriptorPool(pool); }
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

    vk::Result result = m_logicalDevice.AllocateDescriptorSets(&allocInfo, &descriptor);

    if(result == vk::Result::eErrorOutOfPoolMemory)
    {
        m_fullPools.push_back(poolToUse);

        poolToUse = GetPool(m_logicalDevice);
        allocInfo.descriptorPool = poolToUse;

        result = m_logicalDevice.AllocateDescriptorSets(&allocInfo, &descriptor);

        if(result != vk::Result::eSuccess)
        {
            HGERROR("Completely failed to allocate a descriptor set, failing");
            HGERROR("Error: %s", vk::to_string(result).c_str());
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
    for(u32 i = 0; i < static_cast<u32>(m_fullPools.size()); i++) { m_logicalDevice.DestroyDescriptorPool(m_fullPools[i]); }
    m_fullPools.clear();
    for(u32 i = 0; i < static_cast<u32>(m_readyPools.size()); i++) { m_logicalDevice.ResetDescriptorPool(m_readyPools[i]); }
    m_readyPools.clear();
}

vk::DescriptorPool DescriptorPoolGrowable::GetPool(const ILogicalDevice& logicalDevice)
{
    vk::DescriptorPool newPool;
    if(m_readyPools.size() != 0)
    {
        newPool = m_readyPools.back();
        m_readyPools.pop_back();
    }
    else
    {
        newPool = CreatePool(logicalDevice, m_setsPerPool, m_poolTypes, {});

        m_setsPerPool = m_setsPerPool * 1.5;

        if(m_setsPerPool > 4092) { m_setsPerPool = 4092; }
    }

    return newPool;
}

vk::DescriptorPool DescriptorPoolGrowable::CreatePool(const ILogicalDevice& logicalDevice, u32 setCount, std::vector<vk::DescriptorType> poolTypes,
                                                      vk::DescriptorPoolCreateFlags flags) const
{
    std::vector<vk::DescriptorPoolSize> poolSizes;
    for(vk::DescriptorType type: poolTypes) { poolSizes.push_back({type, setCount}); }

    vk::DescriptorPoolCreateInfo info{};
    info.sType = vk::StructureType::eDescriptorPoolCreateInfo;
    info.maxSets = setCount;
    info.poolSizeCount = static_cast<u32>(poolSizes.size());
    info.pPoolSizes = poolSizes.data();
    info.flags = flags;

    vk::DescriptorPool newPool = logicalDevice.CreateDescriptorPool(info);

    return newPool;
}
} // namespace Humongous
