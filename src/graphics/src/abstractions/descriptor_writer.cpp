#include "asserts.hpp"
#include <abstractions/descriptor_writer.hpp>

namespace Humongous
{

DescriptorWriter::DescriptorWriter(DescriptorSetLayout& m_setLayout, DescriptorPool& m_pool) : m_setLayout{m_setLayout}, m_pool{m_pool} {}

DescriptorWriter& DescriptorWriter::WriteBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo)
{
    HGASSERT(m_setLayout.m_bindings.count(binding) == 1 && "Layout does not contain specified binding")

    auto& bindingDescription = m_setLayout.m_bindings[binding];

    HGASSERT(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple")

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pBufferInfo = bufferInfo;
    write.descriptorCount = 1;

    m_writes.push_back(write);
    return *this;
}

DescriptorWriter& DescriptorWriter::WriteImage(uint32_t binding, VkDescriptorImageInfo* imageInfo)
{
    HGASSERT(m_setLayout.m_bindings.count(binding) == 1 && "Layout does not contain specified binding")

    auto& bindingDescription = m_setLayout.m_bindings[binding];

    HGASSERT(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple")

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pImageInfo = imageInfo;
    write.descriptorCount = 1;

    m_writes.push_back(write);
    return *this;
}

bool DescriptorWriter::Build(VkDescriptorSet& set)
{
    bool success = m_pool.AllocateDescriptor(m_setLayout.GetDescriptorSetLayout(), set);
    if(!success) { return false; }
    Overwrite(set);
    return true;
}

void DescriptorWriter::Overwrite(VkDescriptorSet& set)
{
    for(auto& write: m_writes) { write.dstSet = set; }
    vkUpdateDescriptorSets(m_pool.m_logicalDevice.GetVkDevice(), m_writes.size(), m_writes.data(), 0, nullptr);
}

} // namespace Humongous
