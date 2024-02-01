#pragma once

#include "window.hpp"
#include <instance.hpp>
#include <logical_device.hpp>
#include <memory>
#include <physical_device.hpp>

namespace Humongous
{
class VulkanApp
{
public:
    VulkanApp();
    ~VulkanApp();

private:
    std::unique_ptr<Instance>       instance;
    std::unique_ptr<Window>         window;
    std::unique_ptr<PhysicalDevice> physicalDevice;
    std::unique_ptr<LogicalDevice>  logicalDevice;

    void Init();
    void Cleanup();
};
} // namespace Humongous
