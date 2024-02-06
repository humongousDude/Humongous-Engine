#include "asserts.hpp"
#include "logger.hpp"
#include <logical_device.hpp>
#include <set>

namespace Humongous
{
LogicalDevice::LogicalDevice(Instance& instance, PhysicalDevice& physicalDevice) : m_logicalDevice(VK_NULL_HANDLE), m_instance(instance)
{
    CreateLogicalDevice(instance, physicalDevice);
    CreateVmaAllocator(instance, physicalDevice);
}

LogicalDevice::~LogicalDevice()
{
    vmaDestroyAllocator(m_allocator);
    vkDestroyDevice(m_logicalDevice, nullptr);
    HGINFO("Destroyed logical device");
}

void LogicalDevice::CreateLogicalDevice(Instance& instance, PhysicalDevice& physicalDevice)
{
    HGASSERT(m_logicalDevice == VK_NULL_HANDLE && "Logical device has already been made!");
    HGASSERT(physicalDevice.GetVkPhysicalDevice() != VK_NULL_HANDLE && "Can't create a logical device with a null physical device!");

    HGINFO("Creating logical device...");
    PhysicalDevice::QueueFamilyData indices = physicalDevice.FindQueueFamilies(physicalDevice.GetVkPhysicalDevice());

    HGASSERT(indices.IsComplete() && "Incomplete queue family indices!");

    // vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.descriptorIndexing = VK_TRUE;
    vulkan12Features.bufferDeviceAddress = VK_TRUE;

    // vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.synchronization2 = VK_TRUE;
    vulkan13Features.dynamicRendering = VK_TRUE;
    vulkan13Features.pNext = &vulkan12Features;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &vulkan13Features;

    VkPhysicalDeviceFeatures deviceFeatures{};

    auto queueCreateInfos = CreateQueues(physicalDevice);

    // TODO: make queue creation(specifically the aqcuisition of information required for queue creation and aqcuisiton) not fucking ass
    // this also includes the CreateQueues() func, because what it does doesn't match its name
    // ps. also look at the calls tovkGetDeviceQueue2, maybe it can be made better
    //
    // cant be bothered to fix this right now

    std::vector<VkDeviceQueueCreateInfo> queueInfos(2);
    float                                p = 1.0f;

    queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfos[0].queueFamilyIndex = indices.graphicsFamily.value();
    queueInfos[0].queueCount = 1;
    queueInfos[0].pQueuePriorities = &p;

    queueInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfos[1].queueFamilyIndex = indices.presentFamily.value();
    queueInfos[1].queueCount = 1;
    queueInfos[1].pQueuePriorities = &p;

    auto extensions = physicalDevice.GetDeviceExtensions();

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<u32>(queueInfos.size()); // queueCreateInfos.size();
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.enabledLayerCount = 0;
    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
    createInfo.pNext = &vulkan13Features;
    createInfo.pEnabledFeatures = &deviceFeatures;

    if(vkCreateDevice(physicalDevice.GetVkPhysicalDevice(), &createInfo, nullptr, &m_logicalDevice) != VK_SUCCESS)
    {
        HGFATAL("Failed to create logical device!");
    }

    HGINFO("logical device created");

    vkGetDeviceQueue2(m_logicalDevice, &queueCreateInfos[0], &m_graphicsQueue);
    vkGetDeviceQueue2(m_logicalDevice, &queueCreateInfos[1], &m_presentQueue);
    HGINFO("logical device queues acquired");
}

void LogicalDevice::CreateVmaAllocator(Instance& instance, PhysicalDevice& physicalDevice)
{
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = physicalDevice.GetVkPhysicalDevice();
    allocatorInfo.device = m_logicalDevice;
    allocatorInfo.instance = instance.GetVkInstance();
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);
}

std::vector<VkDeviceQueueInfo2> LogicalDevice::CreateQueues(PhysicalDevice& physicalDevice)
{
    HGINFO("acquiring queue handles...");

    PhysicalDevice::QueueFamilyData indices = physicalDevice.FindQueueFamilies(physicalDevice.GetVkPhysicalDevice());

    std::vector<VkDeviceQueueInfo2> queueCreateInfos;
    std::set<u32>                   uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for(u32 queueFamily: uniqueQueueFamilies)
    {
        VkDeviceQueueInfo2 queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueIndex = 0;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    return queueCreateInfos;
}

} // namespace Humongous
