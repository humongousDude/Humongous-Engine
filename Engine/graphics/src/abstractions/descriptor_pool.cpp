// Original from Brendan Galea's vulkan tutorial, adapted to use VMA
#include "logger.hpp"
#include <abstractions/descriptor_pool.hpp>

namespace Humongous
{
// *************** Descriptor Pool Builder *********************

DescriptorPool::Builder& DescriptorPool::Builder::AddPoolSize(vk::DescriptorType descriptorType, u32 count)
{
    m_poolSizes.push_back({descriptorType, count});
    return *this;
}

DescriptorPool::Builder& DescriptorPool::Builder::SetPoolFlags(vk::DescriptorPoolCreateFlagBits flags)
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

DescriptorPool::DescriptorPool(const ILogicalDevice& logicalDevice, u32 m_maxSets, vk::DescriptorPoolCreateFlagBits m_poolFlags,
                               const std::vector<vk::DescriptorPoolSize>& m_poolSizes)
    : m_logicalDevice{logicalDevice}
{
    vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = vk::StructureType::eDescriptorPoolCreateInfo;
    descriptorPoolInfo.poolSizeCount = static_cast<u32>(m_poolSizes.size());
    descriptorPoolInfo.pPoolSizes = m_poolSizes.data();
    descriptorPoolInfo.maxSets = m_maxSets;
    descriptorPoolInfo.flags = m_poolFlags;

    m_descriptorPool = m_logicalDevice.CreateDescriptorPool(descriptorPoolInfo);
}

DescriptorPool::~DescriptorPool() { m_logicalDevice.DestroyDescriptorPool(m_descriptorPool); }

bool DescriptorPool::AllocateDescriptor(const vk::DescriptorSetLayout descriptorSetLayout, vk::DescriptorSet& descriptor) const
{
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = vk::StructureType::eDescriptorSetAllocateInfo;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;

    auto res = m_logicalDevice.AllocateDescriptorSets(&allocInfo, &descriptor);
    if(res != vk::Result::eSuccess)
    {
        HGERROR("Failed to allocate descriptor sets! Error: %s", vk::to_string(res).c_str());
        return false;
    }
    return true;
}

void DescriptorPool::FreeDescriptors(std::vector<vk::DescriptorSet>& descriptors) const
{
    m_logicalDevice.FreeDescriptorSets(m_descriptorPool, descriptors);
}

void DescriptorPool::ResetPool() { m_logicalDevice.ResetDescriptorPool(m_descriptorPool); }
} // namespace Humongous
