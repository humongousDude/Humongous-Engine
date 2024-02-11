#include "logger.hpp"
#include <abstractions/descriptor_pool_growable.hpp>
#include <span>

namespace Humongous
{
DescriptorPoolGrowable::DescriptorPoolGrowable(LogicalDevice& logicalDevice, u32 maxSets, VkDescriptorPoolCreateFlags m_poolFlags,
                                               std::vector<VkDescriptorType>& poolTypes)
    : m_logicalDevice{logicalDevice}
{
    m_poolTypes.clear();

    for(auto t: poolTypes) { m_poolTypes.push_back(t); }

    VkDescriptorPool newPool = CreatePool(logicalDevice, maxSets, poolTypes);

    m_setsPerPool = maxSets * 1.5;

    m_readyPools.push_back(newPool);
}

DescriptorPoolGrowable::~DescriptorPoolGrowable()
{
    for(auto pool: m_readyPools) { vkDestroyDescriptorPool(m_logicalDevice.GetVkDevice(), pool, nullptr); }
    for(auto pool: m_fullPools) { vkDestroyDescriptorPool(m_logicalDevice.GetVkDevice(), pool, nullptr); }
    m_readyPools.clear();
    m_fullPools.clear();
}

bool DescriptorPoolGrowable::AllocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor)
{
    // Get or create a pool to allocate from
    VkDescriptorPool poolToUse = GetPool(m_logicalDevice);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    VkResult result = vkAllocateDescriptorSets(m_logicalDevice.GetVkDevice(), &allocInfo, &descriptor);

    if(result == VK_ERROR_OUT_OF_POOL_MEMORY)
    {
        m_fullPools.push_back(poolToUse);

        poolToUse = GetPool(m_logicalDevice);
        allocInfo.descriptorPool = poolToUse;

        if(vkAllocateDescriptorSets(m_logicalDevice.GetVkDevice(), &allocInfo, &descriptor) != VK_SUCCESS) { return false; }
    }

    m_readyPools.push_back(poolToUse);

    return true;
}

VkDescriptorSet DescriptorPoolGrowable::AllocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout)
{
    VkDescriptorSet descriptor;
    AllocateDescriptor(descriptorSetLayout, descriptor);
    return descriptor;
}

void DescriptorPoolGrowable::ResetPools()
{
    for(int i = 0; i < m_fullPools.size(); i++) { vkResetDescriptorPool(m_logicalDevice.GetVkDevice(), m_fullPools[i], 0); }
    m_fullPools.clear();
    for(int i = 0; i < m_readyPools.size(); i++) { vkResetDescriptorPool(m_logicalDevice.GetVkDevice(), m_readyPools[i], 0); }
    m_readyPools.clear();
}

VkDescriptorPool DescriptorPoolGrowable::GetPool(LogicalDevice& logicalDevice)
{
    VkDescriptorPool newPool;
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

VkDescriptorPool DescriptorPoolGrowable::CreatePool(LogicalDevice& logicalDevice, u32 setCount, std::span<VkDescriptorType> poolTypes) const
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for(VkDescriptorType type: poolTypes) { poolSizes.push_back({type, setCount}); }

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.maxSets = setCount;
    info.poolSizeCount = static_cast<u32>(poolSizes.size());
    info.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    if(vkCreateDescriptorPool(logicalDevice.GetVkDevice(), &info, nullptr, &newPool) != VK_SUCCESS) { HGERROR("Failed to create descriptor pool"); }

    return newPool;
}
} // namespace Humongous
