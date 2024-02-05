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
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    SwapChain(Window& window, PhysicalDevice& physicalDevice, LogicalDevice& logicalDevice, VkSwapchainKHR* oldSwap = nullptr);
    ~SwapChain();

    VkExtent2D       GetExtent() const { return m_extent; }
    VkFormat         GetSurfaceFormat() const { return m_surfaceFormat; }
    VkPresentModeKHR GetPresentMode() const { return m_presentMode; }

    VkResult AcquireNextImage(uint32_t* imageIndex);

    VkSwapchainKHR GetSwapChain() const { return m_swapChain; }

    static void TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    static void CopyImageToImage(VkCommandBuffer cmd, VkImage src, VkImage dst, VkExtent2D srcSize, VkExtent2D dstSize);

    std::vector<VkImageView> GetImageViews() const { return m_imageViews; }
    std::vector<VkImage>     GetImages() const { return m_images; }

private:
    LogicalDevice& m_logicalDevice;
    VkSwapchainKHR m_swapChain;
    // maybe unused?
    // VkSwapchainKHR* m_oldSwap = nullptr;

    VkFormat         m_surfaceFormat;
    VkPresentModeKHR m_presentMode;
    VkExtent2D       m_extent;

    std::vector<VkImage>     m_images;
    std::vector<VkImageView> m_imageViews;

    void CreateSwapChain(Window& window, PhysicalDevice& physicalDevice, VkSwapchainKHR* oldSwap);
    void CreateImageViews();

    VkSurfaceFormat2KHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormat2KHR>& formats);
    VkPresentModeKHR    ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
    VkExtent2D          ChooseExtent(const VkSurfaceCapabilities2KHR& capabilities, Window& window);
};
} // namespace Humongous
