#define VMA_IMPLEMENTATION

#include "logical_device.hpp"
#include "asserts.hpp"
#include "logger.hpp"
#include "vk_mem_alloc.h"
#include <set>

namespace Humongous
{

void VKAPI_PTR VmaAllocateDeviceMemoryFunction(VmaAllocator allocator, uint32_t memoryType, VkDeviceMemory memory, VkDeviceSize size,
                                               void* pUserData)
{
    LogicalDevice::VMAData* myUserData = static_cast<LogicalDevice::VMAData*>(pUserData);
    if(myUserData) { myUserData->allocationCount++; }

    HGTRACE("VMA_ALLOC_CB: Allocated memoryType=%u, memory=0x%p, size=%llu bytes. Total allocations: %d", memoryType, (void*)memory,
            (unsigned long long)size, myUserData ? myUserData->allocationCount : -1);
}

void VKAPI_PTR VmaFreeDeviceMemoryFunction(VmaAllocator allocator, uint32_t memoryType, VkDeviceMemory memory, VkDeviceSize size, void* pUserData)
{
    LogicalDevice::VMAData* myUserData = static_cast<LogicalDevice::VMAData*>(pUserData);
    if(myUserData) { myUserData->freeCount++; }

    HGTRACE("VMA_FREE_CB: Freeing memoryType=%u, memory=0x%p, size=%llu bytes. Total frees: %d", memoryType, (void*)memory,
            (unsigned long long)size, myUserData ? myUserData->freeCount : -1);
}

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

    if(m_vmaData.freeCount < m_vmaData.allocationCount)
    {
        HGERROR("We didn't free every allocation! Allocations: %i, Frees: %i", m_vmaData.allocationCount, m_vmaData.freeCount);
    }
    vkDestroyDevice(m_logicalDevice, nullptr);
    HGINFO("Destroyed logical device");
}

void LogicalDevice::CreateLogicalDevice(Instance& instance, PhysicalDevice& physicalDevice)
{
    HGASSERT(m_logicalDevice == VK_NULL_HANDLE && "Logical device has already been made!");
    HGASSERT(physicalDevice.GetVkPhysicalDevice() != VK_NULL_HANDLE && "Can't create a logical device with a null physical device!");

    PhysicalDevice::QueueFamilyData indices = physicalDevice.FindQueueFamilies(physicalDevice.GetVkPhysicalDevice());

    HGASSERT(indices.IsComplete() && "Incomplete queue family indices!");
    m_graphicsQueueIndex = indices.graphicsFamily.value();
    m_presentQueueIndex = indices.presentFamily.value();

    vk::PhysicalDeviceVulkan11Features vulkan11Features{};
    vulkan11Features.shaderDrawParameters = VK_TRUE;

    // vulkan 1.2 features
    vk::PhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.descriptorIndexing = VK_TRUE;
    vulkan12Features.bufferDeviceAddress = VK_TRUE;
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
    vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    vulkan12Features.samplerFilterMinmax = VK_TRUE;
    vulkan12Features.pNext = &vulkan11Features;

    // vulkan 1.3 features
    vk::PhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.synchronization2 = VK_TRUE;
    vulkan13Features.dynamicRendering = VK_TRUE;
    vulkan13Features.pNext = &vulkan12Features;

    vk::PhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.pNext = &vulkan13Features;

    vk::PhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.multiDrawIndirect = VK_TRUE;

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
    createInfo.queueCreateInfoCount = static_cast<n32>(queueInfos.size()); // queueCreateInfos.size();
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.enabledLayerCount = 0;
    createInfo.enabledExtensionCount = static_cast<n32>(extensions.size());
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

    if(m_graphicsQueueIndex == m_presentQueueIndex)
    {
        m_logicalDevice.getQueue2(&queueCreateInfos[0], &m_graphicsQueue);
        m_logicalDevice.getQueue2(&queueCreateInfos[0], &m_presentQueue);
    }
    else
    {
        m_logicalDevice.getQueue2(&queueCreateInfos[0], &m_graphicsQueue);
        m_logicalDevice.getQueue2(&queueCreateInfos[1], &m_presentQueue);
    }

    HGINFO("logical device queues acquired");
}

void LogicalDevice::CreateVmaAllocator(Instance& instance, PhysicalDevice& physicalDevice)
{
    VmaDeviceMemoryCallbacks memoryCallbacks = {};
    memoryCallbacks.pfnAllocate = VmaAllocateDeviceMemoryFunction;
    memoryCallbacks.pfnFree = VmaFreeDeviceMemoryFunction;
    memoryCallbacks.pUserData = &m_vmaData;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = physicalDevice.GetVkPhysicalDevice();
    allocatorInfo.device = m_logicalDevice;
    allocatorInfo.instance = instance.GetVkInstance();
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorInfo.pDeviceMemoryCallbacks = &memoryCallbacks;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);
}

std::vector<vk::DeviceQueueInfo2> LogicalDevice::CreateQueues(PhysicalDevice& physicalDevice)
{
    HGINFO("acquiring queue handles...");

    PhysicalDevice::QueueFamilyData indices = physicalDevice.FindQueueFamilies(physicalDevice.GetVkPhysicalDevice());

    std::vector<vk::DeviceQueueInfo2> queueCreateInfos;
    std::set<n32>                     uniqueQueueFamilies;
    if(indices.graphicsFamily.has_value()) { uniqueQueueFamilies.insert(indices.graphicsFamily.value()); }
    if(indices.presentFamily.has_value()) { uniqueQueueFamilies.insert(indices.presentFamily.value()); }

    if(uniqueQueueFamilies.empty()) { HGFATAL("Failed to find any suitable queue families!"); }

    float queuePriority = 1.0f;
    for(n32 queueFamily: uniqueQueueFamilies)
    {
        vk::DeviceQueueInfo2 queueCreateInfo{};
        queueCreateInfo.sType = vk::StructureType::eDeviceQueueInfo2;
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
    auto              result = m_logicalDevice.allocateCommandBuffers(&allocInfo, &commandBuffer);
    if(result != vk::Result::eSuccess) { HGERROR("Failed to allocate single time command! Error: %s", vk::to_string(result).c_str()); }

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    result = commandBuffer.begin(&beginInfo);
    if(result != vk::Result::eSuccess) { HGERROR("Failed to start single time command! Error: %s", vk::to_string(result).c_str()); }
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
    auto r = m_graphicsQueue.submit2(1, &submitInfo, VK_NULL_HANDLE);
    if(r != vk::Result::eSuccess) { HGERROR("Failed to submit single time command! Error: %s", vk::to_string(r).c_str()); }

    m_graphicsQueue.waitIdle();
    m_logicalDevice.freeCommandBuffers(m_commandPool, 1, &commandBuffer);
}

} // namespace Humongous
