// Original from Brendan Galea's vulkan tutorial, adapted to use VMA
#pragma once

#include <logical_device.hpp>

#include <memory>

// TODO: Change this to use vulkan.hpp

namespace Humongous
{
class DescriptorPool
{
public:
    class Builder
    {
    public:
        Builder(LogicalDevice& logicalDevice) : m_logicalDevice{logicalDevice} {}

        Builder&                        AddPoolSize(VkDescriptorType descriptorType, n32 count);
        Builder&                        SetPoolFlags(VkDescriptorPoolCreateFlags flags);
        Builder&                        SetMaxSets(n32 count);
        std::unique_ptr<DescriptorPool> Build() const;

    private:
        LogicalDevice&                    m_logicalDevice;
        std::vector<VkDescriptorPoolSize> m_poolSizes{};
        n32                               m_maxSets = 1000;
        VkDescriptorPoolCreateFlags       m_poolFlags = 0;
    };

    DescriptorPool(LogicalDevice& logicalDevice, n32 maxSets, VkDescriptorPoolCreateFlags poolFlags,
                   const std::vector<VkDescriptorPoolSize>& poolSizes);
    ~DescriptorPool();
    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;

    bool AllocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const;
    void FreeDescriptors(std::vector<VkDescriptorSet>& descriptors) const;
    void ResetPool();

    VkDescriptorPool GetRawPoolHandle() const { return m_descriptorPool; }

private:
    LogicalDevice&   m_logicalDevice;
    VkDescriptorPool m_descriptorPool;

    friend class DescriptorWriter;
};
} // namespace Humongous
