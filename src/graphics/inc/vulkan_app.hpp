#pragma once

#include "window.hpp"
#include <instance.hpp>
#include <logical_device.hpp>
#include <memory>
#include <physical_device.hpp>
#include <swapchain.hpp>

namespace Humongous
{
class VulkanApp
{
public:
    VulkanApp();
    ~VulkanApp();

    void Run();

private:
    std::unique_ptr<Instance>       instance;
    std::unique_ptr<Window>         window;
    std::unique_ptr<PhysicalDevice> physicalDevice;
    std::unique_ptr<LogicalDevice>  logicalDevice;
    std::unique_ptr<SwapChain>      swapChain;

    void Init();
    void Cleanup();
};
} // namespace Humongous
