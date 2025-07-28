#pragma once

#include "instance.hpp"
#include "non_copyable.hpp"
#include "window.hpp"
#include <asserts.hpp>
#include <functional>
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

    enum class DeviceSupportLevel
    {
        BaseGraphics,
        MeshShaders,
    };

    struct DeviceCapabilities
    {
        b8 supportsSamplerAnisotropy = false;
        b8 supportsMeshShaders = false;
        b8 supportsBindlessDescriptors = false;
    };

    PhysicalDevice(Instance& instance, Window& window);
    ~PhysicalDevice();

    vk::PhysicalDevice GetVkPhysicalDevice() const { return m_physicalDevice; }

    QueueFamilyData FindQueueFamilies(vk::PhysicalDevice physicalDevice) const;

    std::vector<const char*> GetDeviceExtensions() { return REQUIRED_BASE_DEVICE_EXTENSIONS; }

    SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice physicalDevice) const;

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

    DeviceSupportLevel        GetCurrentSupportLevel() const { return m_currentSupportLevel; }
    const DeviceCapabilities& GetCurrentCapabilities() const { return m_currentCapabilities; }

private:
    Instance&          m_instance;
    vk::PhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    vk::SurfaceKHR     m_surface = VK_NULL_HANDLE;
    DeviceSupportLevel m_currentSupportLevel = DeviceSupportLevel::BaseGraphics;
    DeviceCapabilities m_currentCapabilities;

    void PickPhysicalDevice();

    bool IsDeviceSuitable(vk::PhysicalDevice physicalDevice);
    bool CheckDeviceExtensionSupport(vk::PhysicalDevice physicalDevice);

    b32 CheckExtensionAvailability(vk::PhysicalDevice physicalDevice, const char* extensionName);

    template <typename T>
    b32                CheckPhysicalDeviceFeature(vk::PhysicalDevice physicalDevice, T& featuresStruct, std::function<bool(const T&)> featureCheck);
    DeviceSupportLevel EvaluateDeviceSupportLevel(const DeviceCapabilities& capabilities);
    PhysicalDevice::DeviceCapabilities GetDeviceCapabilities(vk::PhysicalDevice physicalDevice);

    const std::vector<const char*> REQUIRED_BASE_DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
};
} // namespace Humongous
