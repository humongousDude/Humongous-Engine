// Original from Brendan Galea's vulkan tutorial, adapted to use VMA
#pragma once

#include "abstractions/descriptor_pool_growable.hpp"
#include "defines.hpp"
#include <list>
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
    DescriptorWriter(const DescriptorSetLayout& setLayout, DescriptorPool* pool);
    DescriptorWriter(const DescriptorSetLayout& setLayout, DescriptorPoolGrowable* pool);

    DescriptorWriter& WriteBuffer(const n32 binding, vk::DescriptorBufferInfo* bufferInfo);
    DescriptorWriter& WriteImage(const n32 binding, vk::DescriptorImageInfo* imageInfo);
    DescriptorWriter& Write(const vk::WriteDescriptorSet& write)
    {
        m_writes.push_back(write);
        return *this;
    };

    bool Build(vk::DescriptorSet& set);
    void Overwrite(vk::DescriptorSet& set);

private:
    const DescriptorSetLayout&          m_setLayout;
    DescriptorPool*                     m_pool{nullptr};
    DescriptorPoolGrowable*             m_poolGrowable{nullptr};
    std::vector<vk::WriteDescriptorSet> m_writes;

    std::list<vk::DescriptorBufferInfo> m_bufferInfos;
    std::list<vk::DescriptorImageInfo>  m_imageInfos;
};
} // namespace Humongous
