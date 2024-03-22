#include "asserts.hpp"
#include "logger.hpp"
#include <logical_device.hpp>
#include <set>

namespace Humongous
{
LogicalDevice::LogicalDevice(Instance& instance, PhysicalDevice& physicalDevice)
    : m_logicalDevice(VK_NULL_HANDLE), m_instance(instance), m_physicalDevice(&physicalDevice)
{
    HGINFO("Creating logical device...");
    CreateLogicalDevice(instance, physicalDevice);
    CreateVmaAllocator(instance, physicalDevice);
    CreateVmaPool(instance, physicalDevice);
    CreateCommandPool(physicalDevice);
    HGINFO("Created logical device");
}

LogicalDevice::~LogicalDevice()
{
    HGINFO("Destroying logical device...");
    vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);
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
    m_graphicsQueueIndex = indices.graphicsFamily.value();
    m_presentQueueIndex = indices.presentFamily.value();

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
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    auto queueCreateInfos = CreateQueues(physicalDevice);

    // TODO: make queue creation(specifically the acquisition of information required for queue creation and acquisition) not atrocious
    // this also includes the CreateQueues() func, because what it does doesn't match its name
    // ps. also look at the calls to vkGetDeviceQueue2, maybe it can be made better
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
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);
}

void LogicalDevice::CreateVmaPool(Instance& instance, PhysicalDevice& physicalDevice) {}

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

void LogicalDevice::CreateCommandPool(PhysicalDevice& physicalDevice)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = physicalDevice.FindQueueFamilies(physicalDevice.GetVkPhysicalDevice()).graphicsFamily.value();
    if(vkCreateCommandPool(m_logicalDevice, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) { HGFATAL("Failed to create command pool!"); }
}

VkCommandBuffer LogicalDevice::BeginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_logicalDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void LogicalDevice::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkCommandBufferSubmitInfo commandBufferInfo{};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferInfo.commandBuffer = commandBuffer;
    commandBufferInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferInfo;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.pSignalSemaphoreInfos = nullptr;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.pWaitSemaphoreInfos = nullptr;
    submitInfo.pNext = nullptr;

    vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_logicalDevice, m_commandPool, 1, &commandBuffer);
}

} // namespace Humongous
