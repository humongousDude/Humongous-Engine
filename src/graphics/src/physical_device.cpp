#include "logger.hpp"
#include <physical_device.hpp>
#include <set>
#include <string>
#include <vector>

namespace Humongous
{
PhysicalDevice::PhysicalDevice(Instance& instance, Window& window) : m_instance{instance}
{
    window.CreateWindowSurface(m_instance.GetVkInstance(), &m_surface);
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
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance.GetVkInstance(), &deviceCount, nullptr);
    if(deviceCount == 0) { HGFATAL("Failed to find GPUs with Vulkan support!"); }
    HGINFO("found %d devices", deviceCount);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance.GetVkInstance(), &deviceCount, devices.data());

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

PhysicalDevice::SwapChainSupportDetails PhysicalDevice::QuerySwapChainSupport(VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR};
    surfaceInfo.surface = m_surface;
    surfaceInfo.pNext = nullptr;

    SwapChainSupportDetails details{};
    details.capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
    vkGetPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &surfaceInfo, &details.capabilities);

    u32 formatCount;
    vkGetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo, &formatCount, nullptr);

    if(formatCount != 0)
    {
        details.formats.resize(formatCount);
        // FIXME: this is probably not a good way to set the sType, but I can't figure out another way
        for(int i = 0; i < formatCount; i++) { details.formats[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR; }

        vkGetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo, &formatCount, details.formats.data());
    }

    u32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, nullptr);

    if(presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool PhysicalDevice::IsDeviceSuitable(VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceProperties2 deviceProperties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);
    VkPhysicalDeviceFeatures2 deviceFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);

    bool deviceHasFeatures = (deviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
                              deviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) &&
                             deviceFeatures.features.geometryShader;

    bool haveAllRequiredIndices = FindQueueFamilies(physicalDevice).IsComplete();
    bool deviceHasExtensions = CheckDeviceExtensionSupport(physicalDevice);

    bool swapChainAdequate = false;
    if(deviceHasExtensions)
    {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    if(deviceHasFeatures && haveAllRequiredIndices && deviceHasExtensions && swapChainAdequate) { HGINFO("device is suitable"); }

    return deviceHasFeatures && haveAllRequiredIndices && deviceHasExtensions && swapChainAdequate;
}

bool PhysicalDevice::CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice)
{
    u32 extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for(const auto& extension: availableExtensions) { requiredExtensions.erase(extension.extensionName); }

    for(const auto& extension: requiredExtensions) { HGINFO("Missing extension: %s", extension.c_str()); }

    return requiredExtensions.empty();
}

PhysicalDevice::QueueFamilyData PhysicalDevice::FindQueueFamilies(VkPhysicalDevice physicalDevice)
{
    QueueFamilyData indices;
    u32             queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties2> queueFamilyProperties(queueFamilyCount);
    for(auto& queueFamilyProperty: queueFamilyProperties) { queueFamilyProperty.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2; }
    vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    VkBool32 presentSupport = false;

    int i = 0;
    for(const auto& queueFamily: queueFamilyProperties)
    {
        if(queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) { indices.graphicsFamily = i; }
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_surface, &presentSupport);
        if(presentSupport) { indices.presentFamily = i; }
        i++;
    }

    return indices;
}

} // namespace Humongous
