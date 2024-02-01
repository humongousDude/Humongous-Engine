#include "logger.hpp"
#include <physical_device.hpp>
#include <string>
#include <vector>

namespace Humongous
{
PhysicalDevice::PhysicalDevice(VkInstance instance) { PickPhysicalDevice(instance); }

PhysicalDevice::~PhysicalDevice() {}

void PhysicalDevice::PickPhysicalDevice(VkInstance instance)
{
    HGINFO("looking for a physical device...");
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if(deviceCount == 0) { HGFATAL("Failed to find GPUs with Vulkan support!"); }
    HGINFO("found %d devices", deviceCount);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

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

    if(deviceHasFeatures && haveAllRequiredIndices) { HGINFO("device is suitable"); }

    return deviceHasFeatures && haveAllRequiredIndices;
}

PhysicalDevice::QueueFamilyIndices PhysicalDevice::FindQueueFamilies(VkPhysicalDevice physicalDevice)
{
    QueueFamilyIndices indices;
    u32                queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties2> queueFamilyProperties(queueFamilyCount);
    for(auto& queueFamilyProperty: queueFamilyProperties) { queueFamilyProperty.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2; }
    vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    int i = 0;
    for(const auto& queueFamily: queueFamilyProperties)
    {
        if(queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) { indices.graphicsFamily = i; }
        i++;
    }

    return indices;
}

} // namespace Humongous
