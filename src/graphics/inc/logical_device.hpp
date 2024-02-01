#pragma once
#include <defines.hpp>
#include <non_copyable.hpp>

#include <vulkan/vulkan_core.h>

namespace Humongous
{
class LogicalDevice : NonCopyable
{
public:
    LogicalDevice(VkInstance instance, VkPhysicalDevice physicalDevice){};
    ~LogicalDevice(){};

    VkDevice GetLogicalDevice() const { return logicalDevice; }

private:
    VkDevice logicalDevice;
};
} // namespace Humongous
