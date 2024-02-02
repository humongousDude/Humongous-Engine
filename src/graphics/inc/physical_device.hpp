#pragma once

#include "instance.hpp"
#include "non_copyable.hpp"
#include "window.hpp"
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
        std::optional<u32> presentFamily;

        bool IsComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    PhysicalDevice(Instance& instance, Window& window);
    ~PhysicalDevice();

    VkPhysicalDevice GetVkPhysicalDevice() const { return m_physicalDevice; }

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice);

private:
    Instance&        m_instance;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR     m_surface = VK_NULL_HANDLE;

    void PickPhysicalDevice();

    bool IsDeviceSuitable(VkPhysicalDevice physicalDevice);
};
} // namespace Humongous
