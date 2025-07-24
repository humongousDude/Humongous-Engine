// Original from Brendan Galea's vulkan tutorial, adapted to use VMA
#include "asserts.hpp"
#include "logger.hpp"
#include <abstractions/descriptor_layout.hpp>
#include <abstractions/descriptor_pool.hpp>

namespace Humongous
{

// *************** Descriptor Set Layout Builder *********************

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::AddBinding(n32 binding, vk::DescriptorType descriptorType,
                                                                       vk::ShaderStageFlags stageFlags, n32 count)
{
    HGASSERT(m_bindings.count(binding) == 0 && "Binding already in use")
    vk::DescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = descriptorType;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    m_bindings[binding] = layoutBinding;
    return *this;
}

std::unique_ptr<DescriptorSetLayout> DescriptorSetLayout::Builder::Build() const
{
    return std::make_unique<DescriptorSetLayout>(m_device, m_bindings);
}

// *************** Descriptor Set Layout *********************

DescriptorSetLayout::DescriptorSetLayout(LogicalDevice& m_device, std::unordered_map<n32, vk::DescriptorSetLayoutBinding> m_bindings)
    : m_device{m_device}, m_bindings{m_bindings}
{
    std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings{};
    for(auto kv: m_bindings) { setLayoutBindings.push_back(kv.second); }

    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = vk::StructureType::eDescriptorSetLayoutCreateInfo;
    descriptorSetLayoutInfo.bindingCount = static_cast<n32>(setLayoutBindings.size());
    descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

    if(m_device.GetVkDevice().createDescriptorSetLayout(&descriptorSetLayoutInfo, nullptr, &m_descriptorSetLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create descriptor set layout!");
    }
}

DescriptorSetLayout::~DescriptorSetLayout() { m_device.GetVkDevice().destroyDescriptorSetLayout(m_descriptorSetLayout, nullptr); }

} // namespace Humongous
