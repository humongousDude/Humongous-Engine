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
        VkSurfaceCapabilities2KHR        capabilities;
        std::vector<VkSurfaceFormat2KHR> formats;
        std::vector<VkPresentModeKHR>    presentModes;
    };

    PhysicalDevice(Instance& instance, Window& window);
    ~PhysicalDevice();

    VkPhysicalDevice GetVkPhysicalDevice() const { return m_physicalDevice; }

    QueueFamilyData FindQueueFamilies(VkPhysicalDevice physicalDevice);

    std::vector<const char*> GetDeviceExtensions() { return deviceExtensions; }

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice);

    VkSurfaceKHR GetSurface() const { return m_surface; }

    VkPhysicalDeviceProperties2 GetProperties() const
    {
        VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        vkGetPhysicalDeviceProperties2(m_physicalDevice, &properties);
        return properties;
    }

    VkPhysicalDeviceFeatures2 GetFeatures() const
    {
        VkPhysicalDeviceFeatures2 features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features);
        return features;
    }

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
