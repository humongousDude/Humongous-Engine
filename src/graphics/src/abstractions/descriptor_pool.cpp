#include "logger.hpp"
#include <abstractions/descriptor_pool.hpp>

namespace Humongous
{
// *************** Descriptor Pool Builder *********************

DescriptorPool::Builder& DescriptorPool::Builder::AddPoolSize(VkDescriptorType descriptorType, u32 count)
{
    m_poolSizes.push_back({descriptorType, count});
    return *this;
}

DescriptorPool::Builder& DescriptorPool::Builder::SetPoolFlags(VkDescriptorPoolCreateFlags flags)
{
    m_poolFlags = flags;
    return *this;
}
DescriptorPool::Builder& DescriptorPool::Builder::SetMaxSets(u32 count)
{
    m_maxSets = count;
    return *this;
}

std::unique_ptr<DescriptorPool> DescriptorPool::Builder::Build() const
{
    return std::make_unique<DescriptorPool>(m_logicalDevice, m_maxSets, m_poolFlags, m_poolSizes);
}

// *************** Descriptor Pool *********************

DescriptorPool::DescriptorPool(LogicalDevice& logicalDevice, u32 m_maxSets, VkDescriptorPoolCreateFlags m_poolFlags,
                               const std::vector<VkDescriptorPoolSize>& m_poolSizes)
    : m_logicalDevice{logicalDevice}
{
    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<u32>(m_poolSizes.size());
    descriptorPoolInfo.pPoolSizes = m_poolSizes.data();
    descriptorPoolInfo.maxSets = m_maxSets;
    descriptorPoolInfo.flags = m_poolFlags;

    if(vkCreateDescriptorPool(m_logicalDevice.GetVkDevice(), &descriptorPoolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
    {
        HGERROR("Failed to create descriptor pool!");
    }
}

DescriptorPool::~DescriptorPool() { vkDestroyDescriptorPool(m_logicalDevice.GetVkDevice(), m_descriptorPool, nullptr); }

bool DescriptorPool::AllocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    // might want to create a "DescriptorPoolManager" class that handles this case, and builds
    // a new pool whenever an old pool fills up. will do later

    /* if(vkAllocateDescriptorSets(m_logicalDevice.GetVkDevice(), &allocInfo, &descriptor) != VK_SUCCESS) { return false; }
    return true; */
    VkResult result = vkAllocateDescriptorSets(m_logicalDevice.GetVkDevice(), &allocInfo, &descriptor);
    return result == VK_SUCCESS;
}

void DescriptorPool::FreeDescriptors(std::vector<VkDescriptorSet>& descriptors) const
{
    vkFreeDescriptorSets(m_logicalDevice.GetVkDevice(), m_descriptorPool, static_cast<u32>(descriptors.size()), descriptors.data());
}

void DescriptorPool::ResetPool() { vkResetDescriptorPool(m_logicalDevice.GetVkDevice(), m_descriptorPool, 0); }
} // namespace Humongous
