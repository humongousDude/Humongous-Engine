#pragma once

#include "logical_device.hpp"
#include "non_copyable.hpp"
#include "physical_device.hpp"
#include "window.hpp"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Humongous
{
class SwapChain : NonCopyable
{
public:
    SwapChain(Window& window, PhysicalDevice& physicalDevice, LogicalDevice& logicalDevice, VkSwapchainKHR* oldSwap = nullptr);
    ~SwapChain();

private:
    VkSwapchainKHR m_swapChain;
    LogicalDevice& m_logicalDevice;
    // maybe unused?
    // VkSwapchainKHR* m_oldSwap = nullptr;

    VkFormat         m_surfaceFormat;
    VkPresentModeKHR m_presentMode;
    VkExtent2D       m_extent;

    void CreateSwapChain(Window& window, PhysicalDevice& physicalDevice, VkSwapchainKHR* oldSwap);

    VkSurfaceFormat2KHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormat2KHR>& formats);
    VkPresentModeKHR    ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
    VkExtent2D          ChooseExtent(const VkSurfaceCapabilities2KHR& capabilities, Window& window);
};
} // namespace Humongous
