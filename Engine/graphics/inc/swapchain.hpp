#pragma once

#include "logical_device.hpp"
#include "non_copyable.hpp"
#include "physical_device.hpp"
#include "window.hpp"
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Humongous
{
class SwapChain : NonCopyable
{
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    SwapChain(Window& window, PhysicalDevice& physicalDevice, LogicalDevice& logicalDevice, std::shared_ptr<SwapChain> oldSwap = nullptr);
    ~SwapChain();

    vk::Extent2D       GetExtent() const { return m_extent; }
    vk::Format         GetSurfaceFormat() const { return m_surfaceFormat; }
    vk::PresentModeKHR GetPresentMode() const { return m_presentMode; }

    vk::Result AcquireNextImage(uint32_t* imageIndex);

    vk::SwapchainKHR GetSwapChain() const { return m_swapChain; }

    const bool CompareSwapFormats(const SwapChain& swapChain)
    {
        return swapChain.GetSurfaceFormat() == m_surfaceFormat && swapChain.GetPresentMode() == m_presentMode;
    }

    std::vector<vk::ImageView> GetImageViews() const { return m_imageViews; }
    std::vector<vk::Image>     GetImages() const { return m_images; }

private:
    LogicalDevice&   m_logicalDevice;
    vk::SwapchainKHR m_swapChain;

    vk::Format         m_surfaceFormat;
    vk::PresentModeKHR m_presentMode;
    vk::Extent2D       m_extent;

    std::vector<vk::Image>     m_images;
    std::vector<vk::ImageView> m_imageViews;

    void CreateSwapChain(Window& window, PhysicalDevice& physicalDevice, vk::SwapchainKHR* oldSwap = nullptr);
    void CreateImageViews();

    vk::SurfaceFormat2KHR ChooseSurfaceFormat(const std::vector<vk::SurfaceFormat2KHR>& formats);
    vk::PresentModeKHR    ChoosePresentMode(const std::vector<vk::PresentModeKHR>& presentModes);
    vk::Extent2D          ChooseExtent(const vk::SurfaceCapabilities2KHR& capabilities, Window& window);
};
} // namespace Humongous
