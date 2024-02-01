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

private:
    VkDevice m_logicalDevice = VK_NULL_HANDLE;

    void CreateLogicalDevice(Instance& instance, PhysicalDevice& physicalDevice);
};
} // namespace Humongous
