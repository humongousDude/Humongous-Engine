// Original from Brendan Galea's vulkan tutorial, adapted to use VMA
#pragma once

#include "logical_device.hpp"
#include <memory>
#include <unordered_map>

// TODO: Change this to use vulkan.hpp
namespace Humongous
{
class DescriptorSetLayout
{
public:
    class Builder
    {
    public:
        Builder(LogicalDevice& device) : m_device{device} {}

        Builder&                             addBinding(u32 binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, u32 count = 1);
        std::unique_ptr<DescriptorSetLayout> build() const;

    private:
        LogicalDevice&                                             m_device;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindings{};
    };

    DescriptorSetLayout(LogicalDevice& device, std::unordered_map<u32, VkDescriptorSetLayoutBinding> bindings);
    ~DescriptorSetLayout();
    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }

private:
    LogicalDevice&                                        m_device;
    VkDescriptorSetLayout                                 m_descriptorSetLayout;
    std::unordered_map<u32, VkDescriptorSetLayoutBinding> m_bindings;

    friend class DescriptorWriter;
};
} // namespace Humongous
