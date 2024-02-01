#include "logger.hpp"
#include <physical_device.hpp>
#include <vector>

namespace Humongous
{
PhysicalDevice::PhysicalDevice(VkInstance instance) { PickPhysicalDevice(instance); }

PhysicalDevice::~PhysicalDevice() {}

void PhysicalDevice::PickPhysicalDevice(VkInstance instance)
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if(deviceCount == 0) { HGFATAL("Failed to find GPUs with Vulkan support!"); }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for(const auto& device: devices)
    {
        if(IsDeviceSuitable(m_physicalDevice))
        {
            m_physicalDevice = device;
            break;
        }
    }

    if(m_physicalDevice == VK_NULL_HANDLE) { HGFATAL("Failed to find a suitable GPU!"); }
}

bool PhysicalDevice::IsDeviceSuitable(VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    bool deviceHasFeatures = (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
                              deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) &&
                             deviceFeatures.geometryShader;

    bool haveAllRequiredIndices = FindQueueFamilies().IsComplete();

    return deviceHasFeatures && haveAllRequiredIndices;
}

PhysicalDevice::QueueFamilyIndices PhysicalDevice::FindQueueFamilies()
{
    QueueFamilyIndices indices;
    u32                queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties2> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    int i = 0;
    for(const auto& queueFamily: queueFamilyProperties)
    {
        if(queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) { indices.graphicsFamily = i; }
        i++;
    }

    return indices;
}

} // namespace Humongous
