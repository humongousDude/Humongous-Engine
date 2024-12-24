#pragma once
#include <defines.hpp>
#include <non_copyable.hpp>

#include "vulkan/vulkan.hpp"
#include <instance.hpp>
#include <physical_device.hpp>

#include <vk_mem_alloc.h>

namespace Humongous
{
class LogicalDevice : NonCopyable
{
public:
    LogicalDevice(Instance& instance, PhysicalDevice& physicalDevice);
    ~LogicalDevice();

    vk::Device      GetVkDevice() const { return m_logicalDevice; }
    PhysicalDevice& GetPhysicalDevice() const { return *m_physicalDevice; }

    vk::Queue GetGraphicsQueue() const { return m_graphicsQueue; }
    vk::Queue GetPresentQueue() const { return m_presentQueue; }

    n32 GetGraphicsQueueIndex() const { return m_graphicsQueueIndex; }
    n32 GetPresentQueueIndex() const { return m_presentQueueIndex; }

    VmaAllocator GetVmaAllocator() const { return m_allocator; }

    vk::CommandBuffer BeginSingleTimeCommands();
    void              EndSingleTimeCommands(vk::CommandBuffer cmd);

private:
    Instance& m_instance;

    vk::Device      m_logicalDevice = VK_NULL_HANDLE;
    PhysicalDevice* m_physicalDevice;

    vk::Queue m_graphicsQueue;
    vk::Queue m_presentQueue;
    n32       m_graphicsQueueIndex;
    n32       m_presentQueueIndex;

    VmaAllocator m_allocator;

    vk::CommandPool m_commandPool;

    void CreateLogicalDevice(Instance& instance, PhysicalDevice& physicalDevice);
    void CreateVmaAllocator(Instance& instance, PhysicalDevice& physicalDevice);
    void CreateCommandPool(PhysicalDevice& physicalDevice);

    std::vector<vk::DeviceQueueInfo2> CreateQueues(PhysicalDevice& physicalDevice);
};
} // namespace Humongous
