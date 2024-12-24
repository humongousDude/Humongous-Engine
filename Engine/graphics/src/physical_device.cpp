#include "logger.hpp"
#include "set"
#include "string"
#include "vector"
#include <physical_device.hpp>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_to_string.hpp>

// FIXME: No discard warnings

namespace Humongous
{
PhysicalDevice::PhysicalDevice(Instance& instance, Window& window) : m_instance{instance}
{
    m_surface = window.CreateWindowSurface(m_instance.GetVkInstance());
    PickPhysicalDevice();
}

PhysicalDevice::~PhysicalDevice()
{
    vkDestroySurfaceKHR(m_instance.GetVkInstance(), m_surface, nullptr);
    HGINFO("Destroyed surface and let go of physical device handle");
}

void PhysicalDevice::PickPhysicalDevice()
{
    HGINFO("looking for a physical device...");
    n32 deviceCount = 0;
    m_instance.GetVkInstance().enumeratePhysicalDevices(&deviceCount, nullptr);
    if(deviceCount == 0) { HGFATAL("Failed to find GPUs with Vulkan support!"); }
    HGINFO("found %d devices", deviceCount);
    std::vector<vk::PhysicalDevice> devices(deviceCount);
    m_instance.GetVkInstance().enumeratePhysicalDevices(&deviceCount, devices.data());

    for(const auto& device: devices)
    {
        HGINFO("checking device");
        if(IsDeviceSuitable(device))
        {
            m_physicalDevice = device;
            HGINFO("found a suitable physical device!");
            break;
        }
        HGINFO("device not suitable");
    }

    HGASSERT(m_physicalDevice != VK_NULL_HANDLE && "Failed to find a suitable GPU!");
}

PhysicalDevice::SwapChainSupportDetails PhysicalDevice::QuerySwapChainSupport(vk::PhysicalDevice physicalDevice)
{
    vk::PhysicalDeviceSurfaceInfo2KHR surfaceInfo{};
    surfaceInfo.surface = m_surface;
    surfaceInfo.pNext = nullptr;

    SwapChainSupportDetails details{};
    if(physicalDevice.getSurfaceCapabilities2KHR(&surfaceInfo, &details.capabilities) != vk::Result::eSuccess)
    {
        HGFATAL("Failed to get surface capabilities!");
    };

    n32 formatCount;
    physicalDevice.getSurfaceFormats2KHR(&surfaceInfo, &formatCount, nullptr);

    if(formatCount != 0)
    {
        details.formats.resize(formatCount);
        // FIXME: this is probably not a good way to set the sType, but I can't figure out another way
        for(int i = 0; i < formatCount; i++) { details.formats[i].sType = vk::StructureType::eSurfaceFormat2KHR; }

        physicalDevice.getSurfaceFormats2KHR(&surfaceInfo, &formatCount, details.formats.data());
    }

    n32 presentModeCount;
    physicalDevice.getSurfacePresentModesKHR(m_surface, &presentModeCount, nullptr);

    if(presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        physicalDevice.getSurfacePresentModesKHR(m_surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool PhysicalDevice::IsDeviceSuitable(vk::PhysicalDevice physicalDevice)
{
    vk::PhysicalDeviceProperties2 deviceProperties{};
    physicalDevice.getProperties2(&deviceProperties);
    vk::PhysicalDeviceFeatures2 deviceFeatures{};
    physicalDevice.getFeatures2(&deviceFeatures);

    bool deviceHasFeatures = (deviceProperties.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ||
                              deviceProperties.properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) &&
                             deviceFeatures.features.geometryShader;

    bool haveAllRequiredIndices = FindQueueFamilies(physicalDevice).IsComplete();
    bool deviceHasExtensions = CheckDeviceExtensionSupport(physicalDevice);

    bool swapChainAdequate = false;
    if(deviceHasExtensions)
    {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    if(deviceHasFeatures && haveAllRequiredIndices && deviceHasExtensions && swapChainAdequate)
    {
        HGINFO("device is suitable");
        HGINFO("Device: %s", deviceProperties.properties.deviceName.data());
    }

    return deviceHasFeatures && haveAllRequiredIndices && deviceHasExtensions && swapChainAdequate;
}

bool PhysicalDevice::CheckDeviceExtensionSupport(vk::PhysicalDevice physicalDevice)
{
    n32 extensionCount;
    physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<vk::ExtensionProperties> availableExtensions(extensionCount);
    physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    HGDEBUG("%d extensions avablialbi", extensionCount);

    for(const auto& extension: availableExtensions) { requiredExtensions.erase(extension.extensionName); }

    for(const auto& extension: requiredExtensions) { HGINFO("Missing extension: %s", extension.c_str()); }

    return requiredExtensions.empty();
}

PhysicalDevice::QueueFamilyData PhysicalDevice::FindQueueFamilies(vk::PhysicalDevice physicalDevice)
{
    QueueFamilyData indices;
    n32             queueFamilyCount = 0;
    physicalDevice.getQueueFamilyProperties2(&queueFamilyCount, nullptr);
    std::vector<vk::QueueFamilyProperties2> queueFamilyProperties(queueFamilyCount);
    for(auto& queueFamilyProperty: queueFamilyProperties) { queueFamilyProperty.sType = vk::StructureType::eQueueFamilyProperties2; }
    physicalDevice.getQueueFamilyProperties2(&queueFamilyCount, queueFamilyProperties.data());
    vk::Bool32 presentSupport = false;

    int i = 0;
    for(const auto& queueFamily: queueFamilyProperties)
    {
        if(queueFamily.queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) { indices.graphicsFamily = i; }
        physicalDevice.getSurfaceSupportKHR(i, m_surface, &presentSupport);
        if(presentSupport) { indices.presentFamily = i; }
        i++;
    }
    HGINFO("Indices graphics: %d, present: %d", indices.graphicsFamily.value(), indices.presentFamily.value());

    return indices;
}

} // namespace Humongous
