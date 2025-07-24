// Original from Brendan Galea's vulkan tutorial, adapted to use VMA
#include "logger.hpp"
#include <abstractions/descriptor_pool.hpp>

namespace Humongous
{
// *************** Descriptor Pool Builder *********************

DescriptorPool::Builder& DescriptorPool::Builder::AddPoolSize(vk::DescriptorType descriptorType, n32 count)
{
    m_poolSizes.push_back({descriptorType, count});
    return *this;
}

DescriptorPool::Builder& DescriptorPool::Builder::SetPoolFlags(vk::DescriptorPoolCreateFlagBits flags)
{
    m_poolFlags = flags;
    return *this;
}
DescriptorPool::Builder& DescriptorPool::Builder::SetMaxSets(n32 count)
{
    m_maxSets = count;
    return *this;
}

std::unique_ptr<DescriptorPool> DescriptorPool::Builder::Build() const
{
    return std::make_unique<DescriptorPool>(m_logicalDevice, m_maxSets, m_poolFlags, m_poolSizes);
}

// *************** Descriptor Pool *********************

DescriptorPool::DescriptorPool(LogicalDevice& logicalDevice, n32 m_maxSets, vk::DescriptorPoolCreateFlagBits m_poolFlags,
                               const std::vector<vk::DescriptorPoolSize>& m_poolSizes)
    : m_logicalDevice{logicalDevice}
{
    vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = vk::StructureType::eDescriptorPoolCreateInfo;
    descriptorPoolInfo.poolSizeCount = static_cast<n32>(m_poolSizes.size());
    descriptorPoolInfo.pPoolSizes = m_poolSizes.data();
    descriptorPoolInfo.maxSets = m_maxSets;
    descriptorPoolInfo.flags = m_poolFlags;

    if(m_logicalDevice.GetVkDevice().createDescriptorPool(&descriptorPoolInfo, nullptr, &m_descriptorPool) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create descriptor pool!");
    }
}

DescriptorPool::~DescriptorPool() { m_logicalDevice.GetVkDevice().destroyDescriptorPool(m_descriptorPool, nullptr); }

bool DescriptorPool::AllocateDescriptor(const vk::DescriptorSetLayout descriptorSetLayout, vk::DescriptorSet& descriptor) const
{
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = vk::StructureType::eDescriptorSetAllocateInfo;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    // might want to create a "DescriptorPoolManager" class that handles this case, and builds
    // a new pool whenever an old pool fills up. will do later

    /* if(vkAllocateDescriptorSets(m_logicalDevice.Getvk::Device(), &allocInfo, &descriptor) != VK_SUCCESS) { return false; }
    return true; */
    vk::Result result = m_logicalDevice.GetVkDevice().allocateDescriptorSets(&allocInfo, &descriptor);
    return result == vk::Result::eSuccess;
}

void DescriptorPool::FreeDescriptors(std::vector<vk::DescriptorSet>& descriptors) const
{
    m_logicalDevice.GetVkDevice().freeDescriptorSets(m_descriptorPool, static_cast<n32>(descriptors.size()), descriptors.data());
}

void DescriptorPool::ResetPool() { m_logicalDevice.GetVkDevice().resetDescriptorPool(m_descriptorPool); }
} // namespace Humongous
