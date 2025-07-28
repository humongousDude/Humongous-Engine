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
        Builder(const LogicalDevice& logicalDevice) : m_logicalDevice{logicalDevice} {}

        Builder&                        AddPoolSize(vk::DescriptorType descriptorType, u32 count);
        Builder&                        SetPoolFlags(vk::DescriptorPoolCreateFlagBits flags);
        Builder&                        SetMaxSets(u32 count);
        std::unique_ptr<DescriptorPool> Build() const;

    private:
        const LogicalDevice&                m_logicalDevice;
        std::vector<vk::DescriptorPoolSize> m_poolSizes{};
        u32                                 m_maxSets = 1000;
        vk::DescriptorPoolCreateFlagBits    m_poolFlags{};
    };

    DescriptorPool(const LogicalDevice& logicalDevice, u32 maxSets, vk::DescriptorPoolCreateFlagBits poolFlags,
                   const std::vector<vk::DescriptorPoolSize>& poolSizes);
    ~DescriptorPool();
    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;

    bool AllocateDescriptor(const vk::DescriptorSetLayout descriptorSetLayout, vk::DescriptorSet& descriptor) const;
    void FreeDescriptors(std::vector<vk::DescriptorSet>& descriptors) const;
    void ResetPool();

    vk::DescriptorPool GetRawPoolHandle() const { return m_descriptorPool; }

private:
    const LogicalDevice& m_logicalDevice;
    vk::DescriptorPool   m_descriptorPool;

    friend class DescriptorWriter;
};
} // namespace Humongous
