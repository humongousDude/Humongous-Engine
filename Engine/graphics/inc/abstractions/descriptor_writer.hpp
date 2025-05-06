// Original from Brendan Galea's vulkan tutorial, adapted to use VMA
#pragma once

#include "abstractions/descriptor_pool_growable.hpp"
#include "defines.hpp"
#include <logical_device.hpp>
#include <non_copyable.hpp>

#include <abstractions/descriptor_layout.hpp>
#include <abstractions/descriptor_pool.hpp>

// TODO: Change this to use vulkan.hpp
namespace Humongous
{
class DescriptorWriter : NonCopyable
{
public:
    DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool* pool);
    DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPoolGrowable* pool);

    DescriptorWriter& WriteBuffer(n32 binding, vk::DescriptorBufferInfo* bufferInfo);
    DescriptorWriter& WriteImage(n32 binding, vk::DescriptorImageInfo* imageInfo);

    bool Build(vk::DescriptorSet& set);
    void Overwrite(vk::DescriptorSet& set);

private:
    DescriptorSetLayout&                m_setLayout;
    DescriptorPool*                     m_pool{nullptr};
    DescriptorPoolGrowable*             m_poolGrowable{nullptr};
    std::vector<vk::WriteDescriptorSet> m_writes;
};
} // namespace Humongous
