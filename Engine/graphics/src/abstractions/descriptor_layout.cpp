// Original from Brendan Galea's vulkan tutorial, adapted to use VMA
#include "abstractions/descriptor_layout.hpp"
#include "asserts.hpp"
#include "logger.hpp"

namespace Humongous
{

// *************** Descriptor Set Layout Builder *********************

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::AddBinding(u32 binding, vk::DescriptorType descriptorType,
                                                                       vk::ShaderStageFlags stageFlags, u32 count)
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

DescriptorSetLayout::DescriptorSetLayout(const ILogicalDevice& m_device, std::unordered_map<u32, vk::DescriptorSetLayoutBinding> m_bindings)
    : m_device{m_device}, m_bindings{m_bindings}
{
    std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings{};
    for(auto kv: m_bindings) { setLayoutBindings.push_back(kv.second); }

    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = vk::StructureType::eDescriptorSetLayoutCreateInfo;
    descriptorSetLayoutInfo.bindingCount = static_cast<u32>(setLayoutBindings.size());
    descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

    m_device.CreateDescriptorSetLayout(descriptorSetLayoutInfo, &m_descriptorSetLayout);
}

DescriptorSetLayout::~DescriptorSetLayout() { m_device.DestroyDescriptorSetLayout(m_descriptorSetLayout); }

} // namespace Humongous
