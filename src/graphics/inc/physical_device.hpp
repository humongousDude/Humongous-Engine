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
    struct QueueFamilyData
    {
        std::optional<u32> graphicsFamily;
        std::optional<u32> presentFamily;

        bool IsComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    PhysicalDevice(Instance& instance, Window& window);
    ~PhysicalDevice();

    VkPhysicalDevice GetVkPhysicalDevice() const { return m_physicalDevice; }

    QueueFamilyData FindQueueFamilies(VkPhysicalDevice physicalDevice);

    std::vector<const char*> GetDeviceExtensions() { return deviceExtensions; }

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice);

private:
    Instance&        m_instance;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR     m_surface = VK_NULL_HANDLE;

    void PickPhysicalDevice();

    bool IsDeviceSuitable(VkPhysicalDevice physicalDevice);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice);

    const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};
} // namespace Humongous
