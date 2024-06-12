#include "asserts.hpp"
#include "logger.hpp"
#include <logical_device.hpp>
#include <set>

// FIXME: No discard warnings

namespace Humongous
{
LogicalDevice::LogicalDevice(Instance& instance, PhysicalDevice& physicalDevice)
    : m_logicalDevice{VK_NULL_HANDLE}, m_instance{instance}, m_physicalDevice{&physicalDevice}
{
    HGINFO("Creating logical device...");
    CreateLogicalDevice(instance, physicalDevice);
    CreateVmaAllocator(instance, physicalDevice);
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
    vk::PhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.descriptorIndexing = VK_TRUE;
    vulkan12Features.bufferDeviceAddress = VK_TRUE;

    // vulkan 1.3 features
    vk::PhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.synchronization2 = VK_TRUE;
    vulkan13Features.dynamicRendering = VK_TRUE;
    vulkan13Features.pNext = &vulkan12Features;

    vk::PhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.pNext = &vulkan13Features;

    vk::PhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    auto queueCreateInfos = CreateQueues(physicalDevice);

    // TODO: make queue creation(specifically the acquisition of information required for queue creation and acquisition) not atrocious
    // this also includes the CreateQueues() func, because what it does doesn't match its name
    // ps. also look at the calls to vkGetDeviceQueue2, maybe it can be made better
    //
    // cant be bothered to fix this right now

    std::vector<vk::DeviceQueueCreateInfo> queueInfos(2);
    float                                  p = 1.0f;

    queueInfos[0].queueFamilyIndex = indices.graphicsFamily.value();
    queueInfos[0].queueCount = 1;
    queueInfos[0].pQueuePriorities = &p;

    queueInfos[1].queueFamilyIndex = indices.presentFamily.value();
    queueInfos[1].queueCount = 1;
    queueInfos[1].pQueuePriorities = &p;

    auto extensions = physicalDevice.GetDeviceExtensions();

    vk::DeviceCreateInfo createInfo{};
    createInfo.queueCreateInfoCount = static_cast<u32>(queueInfos.size()); // queueCreateInfos.size();
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.enabledLayerCount = 0;
    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
    createInfo.pNext = &vulkan13Features;
    createInfo.pEnabledFeatures = &deviceFeatures;

    if(physicalDevice.GetVkPhysicalDevice().createDevice(&createInfo, nullptr, &m_logicalDevice) != vk::Result::eSuccess)
    {
        HGFATAL("Failed to create logical device!");
    }

    HGINFO("logical device created");

    m_logicalDevice.getQueue2(&queueCreateInfos[0], &m_graphicsQueue);
    m_logicalDevice.getQueue2(&queueCreateInfos[1], &m_presentQueue);

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

std::vector<vk::DeviceQueueInfo2> LogicalDevice::CreateQueues(PhysicalDevice& physicalDevice)
{
    HGINFO("acquiring queue handles...");

    PhysicalDevice::QueueFamilyData indices = physicalDevice.FindQueueFamilies(physicalDevice.GetVkPhysicalDevice());

    std::vector<vk::DeviceQueueInfo2> queueCreateInfos;
    std::set<u32>                     uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for(u32 queueFamily: uniqueQueueFamilies)
    {
        vk::DeviceQueueInfo2 queueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueIndex = 0;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    return queueCreateInfos;
}

void LogicalDevice::CreateCommandPool(PhysicalDevice& physicalDevice)
{
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = physicalDevice.FindQueueFamilies(physicalDevice.GetVkPhysicalDevice()).graphicsFamily.value();

    if(m_logicalDevice.createCommandPool(&poolInfo, nullptr, &m_commandPool) != vk::Result::eSuccess) { HGFATAL("Failed to create command pool!"); }
}

vk::CommandBuffer LogicalDevice::BeginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    vk::CommandBuffer commandBuffer{};
    m_logicalDevice.allocateCommandBuffers(&allocInfo, &commandBuffer);

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(&beginInfo);
    return commandBuffer;
}

void LogicalDevice::EndSingleTimeCommands(vk::CommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    vk::CommandBufferSubmitInfo commandBufferInfo{};
    commandBufferInfo.setCommandBuffer(commandBuffer);
    commandBufferInfo.deviceMask = 0;

    vk::SubmitInfo2 submitInfo{};
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferInfo;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.pSignalSemaphoreInfos = nullptr;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.pWaitSemaphoreInfos = nullptr;
    m_graphicsQueue.submit2(1, &submitInfo, VK_NULL_HANDLE);

    m_graphicsQueue.waitIdle();
    m_logicalDevice.freeCommandBuffers(m_commandPool, 1, &commandBuffer);
}

} // namespace Humongous
