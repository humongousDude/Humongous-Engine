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
class IPhysicalDevice
{
public:
    struct QueueFamilyData
    {
        std::optional<u32> graphicsFamily{0};
        std::optional<u32> presentFamily{0};
        std::optional<u32> computeFamily{0};
        std::optional<u32> transferFamily{0};

        b8 IsComplete()
        {
            return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value() && transferFamily.has_value();
        }
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

    virtual ~IPhysicalDevice() = default;
    virtual vk::PhysicalDevice GetVkPhysicalDevice() const = 0;

    virtual QueueFamilyData FindQueueFamilies(vk::PhysicalDevice physicalDevice) const = 0;

    virtual std::vector<const char*> GetDeviceExtensions() const = 0;

    virtual SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice physicalDevice) const = 0;

    virtual vk::SurfaceKHR GetSurface() const = 0;

    virtual vk::PhysicalDeviceProperties2 GetProperties() const = 0;

    virtual VkPhysicalDeviceFeatures2 GetFeatures() const = 0;

    virtual DeviceSupportLevel        GetCurrentSupportLevel() const = 0;
    virtual const DeviceCapabilities& GetCurrentCapabilities() const = 0;
};

class PhysicalDevice : public IPhysicalDevice, NonCopyable
{

public:
    PhysicalDevice(IInstance& instance, Window& window);
    ~PhysicalDevice();

    vk::PhysicalDevice GetVkPhysicalDevice() const override { return m_physicalDevice; }

    QueueFamilyData FindQueueFamilies(vk::PhysicalDevice physicalDevice) const override;

    std::vector<const char*> GetDeviceExtensions() const override { return m_presentExtensions; }

    SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice physicalDevice) const override;

    vk::SurfaceKHR GetSurface() const override { return m_surface; }

    vk::PhysicalDeviceProperties2 GetProperties() const override
    {
        vk::PhysicalDeviceProperties2 properties;
        properties.sType = vk::StructureType::ePhysicalDeviceProperties2;
        m_physicalDevice.getProperties2(&properties);

        return properties;
    }

    VkPhysicalDeviceFeatures2 GetFeatures() const override
    {
        VkPhysicalDeviceFeatures2 features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features);
        return features;
    }

    DeviceSupportLevel        GetCurrentSupportLevel() const override { return m_currentSupportLevel; }
    const DeviceCapabilities& GetCurrentCapabilities() const override { return m_currentCapabilities; }

private:
    IInstance&         m_instance;
    vk::PhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    vk::SurfaceKHR     m_surface = VK_NULL_HANDLE;
    DeviceSupportLevel m_currentSupportLevel = DeviceSupportLevel::BaseGraphics;
    DeviceCapabilities m_currentCapabilities;

    void PickPhysicalDevice();

    b32 CheckExtensionAvailability(vk::PhysicalDevice physicalDevice, const char* extensionName);

    template <typename T>
    b32                CheckPhysicalDeviceFeature(vk::PhysicalDevice physicalDevice, T& featuresStruct, std::function<b8(const T&)> featureCheck);
    DeviceSupportLevel EvaluateDeviceSupportLevel(const DeviceCapabilities& capabilities);
    PhysicalDevice::DeviceCapabilities GetDeviceCapabilities(vk::PhysicalDevice physicalDevice);

    const std::vector<const char*> REQUIRED_BASE_DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                                      VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME};
    std::vector<const char*>       m_presentExtensions;
};
} // namespace Humongous
