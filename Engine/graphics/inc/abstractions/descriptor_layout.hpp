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
        Builder(const LogicalDevice& device) : m_device{device} {}

        Builder& AddBinding(u32 binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, u32 count = 1);
        std::unique_ptr<DescriptorSetLayout> Build() const;

    private:
        const LogicalDevice&                                    m_device;
        std::unordered_map<u32, vk::DescriptorSetLayoutBinding> m_bindings{};
    };

    DescriptorSetLayout(const LogicalDevice& device, std::unordered_map<u32, vk::DescriptorSetLayoutBinding> bindings);
    ~DescriptorSetLayout();
    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

    vk::DescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }

    u32 GetBindingCount() const { return m_bindings.size(); }

private:
    const LogicalDevice&                                    m_device;
    vk::DescriptorSetLayout                                 m_descriptorSetLayout;
    std::unordered_map<u32, vk::DescriptorSetLayoutBinding> m_bindings;

    friend class DescriptorWriter;
};
} // namespace Humongous
