#pragma once

#include "non_copyable.hpp"
#include <asserts.hpp>
#include <optional>

#include <vulkan/vulkan_core.h>

namespace Humongous
{
class PhysicalDevice : NonCopyable
{

public:
    struct QueueFamilyIndices
    {
        std::optional<u32> graphicsFamily;

        bool IsComplete() { return graphicsFamily.has_value(); }
    };

    PhysicalDevice(VkInstance instance);
    ~PhysicalDevice();

    VkPhysicalDevice GetVkPhysicalDevice() const { return m_physicalDevice; }

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice);

private:
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    void PickPhysicalDevice(VkInstance instance);

    bool IsDeviceSuitable(VkPhysicalDevice physicalDevice);
};
} // namespace Humongous
