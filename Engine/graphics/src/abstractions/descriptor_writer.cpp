#include "asserts.hpp"
#include <abstractions/descriptor_writer.hpp>

namespace Humongous
{

DescriptorWriter::DescriptorWriter(DescriptorSetLayout& m_setLayout, DescriptorPool* m_pool) : m_setLayout{m_setLayout}, m_pool{m_pool} {}

DescriptorWriter::DescriptorWriter(DescriptorSetLayout& m_setLayout, DescriptorPoolGrowable* m_pool)
    : m_setLayout{m_setLayout}, m_poolGrowable{m_pool}
{
}

DescriptorWriter& DescriptorWriter::WriteBuffer(n32 binding, vk::DescriptorBufferInfo* bufferInfo)
{
    HGASSERT(m_setLayout.m_bindings.count(binding) == 1 && "Layout does not contain specified binding")

    auto& bindingDescription = m_setLayout.m_bindings[binding];

    HGASSERT(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple")

    m_bufferInfos.push_back(*bufferInfo);

    vk::WriteDescriptorSet write{};
    write.sType = vk::StructureType::eWriteDescriptorSet;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pBufferInfo = &m_bufferInfos.back();
    write.descriptorCount = 1;

    m_writes.push_back(write);
    return *this;
}

DescriptorWriter& DescriptorWriter::WriteImage(n32 binding, vk::DescriptorImageInfo* imageInfo)
{
    HGASSERT(m_setLayout.m_bindings.count(binding) == 1 && "Layout does not contain specified binding")

    auto& bindingDescription = m_setLayout.m_bindings[binding];

    HGASSERT(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple")

    m_imageInfos.push_back(*imageInfo);

    vk::WriteDescriptorSet write{};
    write.sType = vk::StructureType::eWriteDescriptorSet;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pImageInfo = &m_imageInfos.back();
    write.descriptorCount = 1;

    m_writes.push_back(write);
    return *this;
}

bool DescriptorWriter::Build(vk::DescriptorSet& set)
{
    bool success;
    if(m_pool) { success = m_pool->AllocateDescriptor(m_setLayout.m_descriptorSetLayout, set); }
    else if(m_poolGrowable) { success = m_poolGrowable->AllocateDescriptor(m_setLayout.m_descriptorSetLayout, set); }

    if(!success) { return false; }
    Overwrite(set);
    return true;
}

void DescriptorWriter::Overwrite(vk::DescriptorSet& set)
{
    for(auto& write: m_writes) { write.dstSet = set; }
    if(m_pool) { m_pool->m_logicalDevice.GetVkDevice().updateDescriptorSets(m_writes.size(), m_writes.data(), 0, nullptr); }
    else { m_poolGrowable->m_logicalDevice.GetVkDevice().updateDescriptorSets(m_writes.size(), m_writes.data(), 0, nullptr); }
}

} // namespace Humongous
