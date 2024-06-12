// Original from Brendan Galea's vulkan tutorial, adapted to use VMA
#include "asserts.hpp"
#include "logger.hpp"
#include <abstractions/descriptor_layout.hpp>
#include <abstractions/descriptor_pool.hpp>

// TODO: Change this to use vulkan.hpp
namespace Humongous
{

// *************** Descriptor Set Layout Builder *********************

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::addBinding(u32 binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags,
                                                                       u32 count)
{
    HGASSERT(m_bindings.count(binding) == 0 && "Binding already in use")
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = descriptorType;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    m_bindings[binding] = layoutBinding;
    return *this;
}

std::unique_ptr<DescriptorSetLayout> DescriptorSetLayout::Builder::build() const
{
    return std::make_unique<DescriptorSetLayout>(m_device, m_bindings);
}

// *************** Descriptor Set Layout *********************

DescriptorSetLayout::DescriptorSetLayout(LogicalDevice& m_device, std::unordered_map<u32, VkDescriptorSetLayoutBinding> m_bindings)
    : m_device{m_device}, m_bindings{m_bindings}
{
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
    for(auto kv: m_bindings) { setLayoutBindings.push_back(kv.second); }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = static_cast<u32>(setLayoutBindings.size());
    descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

    if(vkCreateDescriptorSetLayout(m_device.GetVkDevice(), &descriptorSetLayoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create descriptor set layout!");
    }
}

DescriptorSetLayout::~DescriptorSetLayout() { vkDestroyDescriptorSetLayout(m_device.GetVkDevice(), m_descriptorSetLayout, nullptr); }

} // namespace Humongous
