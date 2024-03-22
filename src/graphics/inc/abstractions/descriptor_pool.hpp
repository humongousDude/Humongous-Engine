#pragma once

#include <logical_device.hpp>

#include <memory>

namespace Humongous
{
class DescriptorPool
{
public:
    class Builder
    {
    public:
        Builder(LogicalDevice& logicalDevice) : m_logicalDevice{logicalDevice} {}

        Builder&                        AddPoolSize(VkDescriptorType descriptorType, u32 count);
        Builder&                        SetPoolFlags(VkDescriptorPoolCreateFlags flags);
        Builder&                        SetMaxSets(u32 count);
        std::unique_ptr<DescriptorPool> Build() const;

    private:
        LogicalDevice&                    m_logicalDevice;
        std::vector<VkDescriptorPoolSize> m_poolSizes{};
        u32                               m_maxSets = 1000;
        VkDescriptorPoolCreateFlags       m_poolFlags = 0;
    };

    DescriptorPool(LogicalDevice& logicalDevice, u32 maxSets, VkDescriptorPoolCreateFlags poolFlags,
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
