#pragma once

#include "instance.hpp"
#include "non_copyable.hpp"
#include "window.hpp"
#include <asserts.hpp>
#include <optional>

#include "vulkan/vulkan_handles.hpp"
#include <vulkan/vulkan.hpp>

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
        vk::SurfaceCapabilities2KHR        capabilities{};
        std::vector<vk::SurfaceFormat2KHR> formats{};
        std::vector<vk::PresentModeKHR>    presentModes{};
    };

    PhysicalDevice(Instance& instance, Window& window);
    ~PhysicalDevice();

    vk::PhysicalDevice GetVkPhysicalDevice() const { return m_physicalDevice; }

    QueueFamilyData FindQueueFamilies(vk::PhysicalDevice physicalDevice);

    std::vector<const char*> GetDeviceExtensions() { return deviceExtensions; }

    SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice physicalDevice);

    vk::SurfaceKHR GetSurface() const { return m_surface; }

    vk::PhysicalDeviceProperties2 GetProperties() const
    {
        vk::PhysicalDeviceProperties2 properties;
        properties.sType = vk::StructureType::ePhysicalDeviceProperties2;
        m_physicalDevice.getProperties2(&properties);

        return properties;
    }

    VkPhysicalDeviceFeatures2 GetFeatures() const
    {
        VkPhysicalDeviceFeatures2 features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features);
        return features;
    }

private:
    Instance&          m_instance;
    vk::PhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    vk::SurfaceKHR     m_surface = VK_NULL_HANDLE;

    void PickPhysicalDevice();

    bool IsDeviceSuitable(vk::PhysicalDevice physicalDevice);
    bool CheckDeviceExtensionSupport(vk::PhysicalDevice physicalDevice);

    const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};
} // namespace Humongous
