#pragma once

#include "non_copyable.hpp"
#include <asserts.hpp>

#include <vulkan/vulkan_core.h>

namespace Humongous
{
class PhysicalDevice : NonCopyable
{
public:
    PhysicalDevice(VkInstance instance);
    ~PhysicalDevice(){};

    VkPhysicalDevice GetVkPhysicalDevice() const { return physicalDevice; }

private:
    VkPhysicalDevice physicalDevice;
};
} // namespace Humongous
