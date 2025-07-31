#pragma once
#include "defines.hpp"
#include "instance.hpp"
#include "non_copyable.hpp"
#include "physical_device.hpp"
#include "vk_mem_alloc.h"

namespace Humongous
{
class ILogicalDevice
{
public:
    virtual ~ILogicalDevice() = default;
    virtual vk::Device      GetVkDevice() const = 0;
    virtual PhysicalDevice& GetPhysicalDevice() const = 0;

    virtual vk::Queue GetGraphicsQueue() const = 0;
    virtual vk::Queue GetPresentQueue() const = 0;

    virtual u32 GetGraphicsQueueIndex() const = 0;
    virtual u32 GetPresentQueueIndex() const = 0;

    virtual VmaAllocator GetVmaAllocator() const = 0;

    virtual vk::CommandBuffer BeginSingleTimeCommands() const = 0;
    virtual void              EndSingleTimeCommands(vk::CommandBuffer cmd) const = 0;

    struct VMAData
    {
        u32 allocationCount = 0;
        u32 freeCount = 0;
    };
};

class LogicalDevice : public ILogicalDevice, NonCopyable
{
public:
    LogicalDevice(Instance& instance, PhysicalDevice& physicalDevice);
    ~LogicalDevice() override;

    vk::Device      GetVkDevice() const override { return m_logicalDevice; }
    PhysicalDevice& GetPhysicalDevice() const override { return *m_physicalDevice; }

    vk::Queue GetGraphicsQueue() const override { return m_graphicsQueue; }
    vk::Queue GetPresentQueue() const override { return m_presentQueue; }

    u32 GetGraphicsQueueIndex() const override { return m_graphicsQueueIndex; }
    u32 GetPresentQueueIndex() const override { return m_presentQueueIndex; }

    VmaAllocator GetVmaAllocator() const override { return m_allocator; }

    vk::CommandBuffer BeginSingleTimeCommands() const override;
    void              EndSingleTimeCommands(vk::CommandBuffer cmd) const override;

    struct VMAData
    {
        u32 allocationCount = 0;
        u32 freeCount = 0;
    };

private:
    Instance& m_instance;

    vk::Device      m_logicalDevice = VK_NULL_HANDLE;
    PhysicalDevice* m_physicalDevice;

    vk::Queue m_graphicsQueue;
    vk::Queue m_presentQueue;
    u32       m_graphicsQueueIndex;
    u32       m_presentQueueIndex;

    VmaAllocator m_allocator;

    VMAData m_vmaData;

    vk::CommandPool m_commandPool;

    void CreateLogicalDevice(Instance& instance, PhysicalDevice& physicalDevice);
    void CreateVmaAllocator(Instance& instance, PhysicalDevice& physicalDevice);
    void CreateCommandPool(PhysicalDevice& physicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> CreateQueues(PhysicalDevice& physicalDevice);
};
} // namespace Humongous
