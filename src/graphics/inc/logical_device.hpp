#pragma once
#include <defines.hpp>
#include <non_copyable.hpp>

#include <instance.hpp>
#include <physical_device.hpp>

namespace Humongous
{
class LogicalDevice : NonCopyable
{
public:
    LogicalDevice(Instance& instance, PhysicalDevice& physicalDevice);
    ~LogicalDevice();

    VkDevice GetVkDevice() const { return m_logicalDevice; }

    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue GetPresentQueue() const { return m_presentQueue; }

private:
    Instance& m_instance;

    VkDevice m_logicalDevice = VK_NULL_HANDLE;

    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;

    void CreateLogicalDevice(Instance& instance, PhysicalDevice& physicalDevice);

    std::vector<VkDeviceQueueInfo2> CreateQueues(PhysicalDevice& physicalDevice);
};
} // namespace Humongous
