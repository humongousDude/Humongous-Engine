// Taken from vkguide.dev
#pragma once

#include "logical_device.hpp"
#include "non_copyable.hpp"

// TODO: Change this to use vulkan.hpp
namespace Humongous

{
class DescriptorPoolGrowable : NonCopyable
{
public:
    DescriptorPoolGrowable(LogicalDevice& logicalDevice, u32 m_maxSets, VkDescriptorPoolCreateFlags m_poolFlags,
                           std::vector<VkDescriptorType>& poolTypes);

    ~DescriptorPoolGrowable();

    int GetPoolCount() { return m_readyPools.size() + m_fullPools.size(); }
    int GetReadyPoolCount() { return m_readyPools.size(); }
    int GetFullPoolCount() { return m_fullPools.size(); }

    bool            AllocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor);
    VkDescriptorSet AllocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout);
    void            ResetPools();

private:
    LogicalDevice&   m_logicalDevice;
    VkDescriptorPool GetPool(LogicalDevice& logicalDevice);
    VkDescriptorPool CreatePool(LogicalDevice& logicalDevice, u32 setCount, std::vector<VkDescriptorType> poolTypes) const;

    std::vector<VkDescriptorType> m_poolTypes;
    std::vector<VkDescriptorPool> m_fullPools, m_readyPools;
    u32                           m_setsPerPool{1};

    friend class DescriptorWriter;
};
} // namespace Humongous
