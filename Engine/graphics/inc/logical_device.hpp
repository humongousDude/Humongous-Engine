#pragma once
#include <defines.hpp>
#include <non_copyable.hpp>

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

    VkDevice        GetVkDevice() const { return m_logicalDevice; }
    PhysicalDevice& GetPhysicalDevice() const { return *m_physicalDevice; }

    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue GetPresentQueue() const { return m_presentQueue; }

    u32 GetGraphicsQueueIndex() const { return m_graphicsQueueIndex; }
    u32 GetPresentQueueIndex() const { return m_presentQueueIndex; }

    VmaAllocator GetVmaAllocator() const { return m_allocator; }

    VkCommandBuffer BeginSingleTimeCommands();
    void            EndSingleTimeCommands(VkCommandBuffer cmd);

private:
    Instance& m_instance;

    VkDevice        m_logicalDevice = VK_NULL_HANDLE;
    PhysicalDevice* m_physicalDevice;

    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    u32     m_graphicsQueueIndex;
    u32     m_presentQueueIndex;

    VmaAllocator m_allocator;

    VkCommandPool m_commandPool;

    void CreateLogicalDevice(Instance& instance, PhysicalDevice& physicalDevice);
    void CreateVmaAllocator(Instance& instance, PhysicalDevice& physicalDevice);
    void CreateCommandPool(PhysicalDevice& physicalDevice);

    std::vector<VkDeviceQueueInfo2> CreateQueues(PhysicalDevice& physicalDevice);
};
} // namespace Humongous
