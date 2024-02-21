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

    VmaAllocator GetVmaAllocator() const { return m_allocator; }

    VkCommandBuffer BeginSingleTimeCommands();
    void            EndSingleTimeCommands(VkCommandBuffer cmd);

private:
    Instance& m_instance;

    VkDevice        m_logicalDevice = VK_NULL_HANDLE;
    PhysicalDevice* m_physicalDevice;

    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;

    VmaAllocator m_allocator;
    // TODO: figure out a better way to manage VMAPools than this
    VmaPool m_vmaPool = VK_NULL_HANDLE;

    VkCommandPool m_commandPool;

    void CreateLogicalDevice(Instance& instance, PhysicalDevice& physicalDevice);
    void CreateVmaAllocator(Instance& instance, PhysicalDevice& physicalDevice);
    void CreateVmaPool(Instance& instance, PhysicalDevice& physicalDevice);
    void CreateCommandPool(PhysicalDevice& physicalDevice);

    std::vector<VkDeviceQueueInfo2> CreateQueues(PhysicalDevice& physicalDevice);
};
} // namespace Humongous
