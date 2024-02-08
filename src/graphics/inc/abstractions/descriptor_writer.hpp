#pragma once

#include <logical_device.hpp>
#include <non_copyable.hpp>

#include <abstractions/descriptor_layout.hpp>
#include <abstractions/descriptor_pool.hpp>

namespace Humongous
{
class DescriptorWriter : NonCopyable
{
public:
    DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool& pool);

    DescriptorWriter& WriteBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
    DescriptorWriter& WriteImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);

    bool Build(VkDescriptorSet& set);
    void Overwrite(VkDescriptorSet& set);

private:
    DescriptorSetLayout&              m_setLayout;
    DescriptorPool&                   m_pool;
    std::vector<VkWriteDescriptorSet> m_writes;
};
} // namespace Humongous