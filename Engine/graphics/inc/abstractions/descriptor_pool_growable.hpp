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
    DescriptorPoolGrowable(const ILogicalDevice& logicalDevice, u32 m_maxSets, vk::DescriptorPoolCreateFlags m_poolFlags,
                           std::vector<vk::DescriptorType>& poolTypes);

    ~DescriptorPoolGrowable();

    int GetPoolCount() const { return m_readyPools.size() + m_fullPools.size(); }
    int GetReadyPoolCount() const { return m_readyPools.size(); }
    int GetFullPoolCount() const { return m_fullPools.size(); }

    bool              AllocateDescriptor(const vk::DescriptorSetLayout descriptorSetLayout, vk::DescriptorSet& descriptor);
    vk::DescriptorSet AllocateDescriptor(const vk::DescriptorSetLayout descriptorSetLayout);
    void              ResetPools();

private:
    const ILogicalDevice& m_logicalDevice;
    vk::DescriptorPool    GetPool(const ILogicalDevice& logicalDevice);
    vk::DescriptorPool    CreatePool(const ILogicalDevice& logicalDevice, u32 setCount, std::vector<vk::DescriptorType> poolTypes,
                                     vk::DescriptorPoolCreateFlags flags) const;

    std::vector<vk::DescriptorType> m_poolTypes;
    std::vector<vk::DescriptorPool> m_fullPools, m_readyPools;
    u32                             m_setsPerPool{1};

    friend class DescriptorWriter;
};
} // namespace Humongous
