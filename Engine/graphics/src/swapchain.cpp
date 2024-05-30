#include "logger.hpp"
#include "logical_device.hpp"
#include "physical_device.hpp"
#include <algorithm>
#include <swapchain.hpp>

namespace Humongous
{
SwapChain::SwapChain(Window& window, PhysicalDevice& physicalDevice, LogicalDevice& logicalDevice, std::shared_ptr<SwapChain> oldSwap)
    : m_logicalDevice(logicalDevice)
{
    auto old = oldSwap == nullptr ? nullptr : oldSwap->m_swapChain;
    CreateSwapChain(window, physicalDevice, &old);
    CreateImageViews();
}

SwapChain::~SwapChain()
{
    for(auto imageView: m_imageViews) { vkDestroyImageView(m_logicalDevice.GetVkDevice(), imageView, nullptr); }

    vkDestroySwapchainKHR(m_logicalDevice.GetVkDevice(), m_swapChain, nullptr);
    HGINFO("Destroyed SwapChain");
}

void SwapChain::CreateSwapChain(Window& window, PhysicalDevice& physicalDevice, vk::SwapchainKHR* oldSwap)
{
    HGINFO("Creating SwapChain...");
    PhysicalDevice::SwapChainSupportDetails details = physicalDevice.QuerySwapChainSupport(physicalDevice.GetVkPhysicalDevice());

    vk::SurfaceFormat2KHR surfaceFormat = ChooseSurfaceFormat(details.formats);
    vk::PresentModeKHR    presentMode = ChoosePresentMode(details.presentModes);
    vk::Extent2D          extent = ChooseExtent(details.capabilities, window);

    u32 imageCount = details.capabilities.surfaceCapabilities.minImageCount + 1;
    if(details.capabilities.surfaceCapabilities.maxImageCount > 0 && imageCount > details.capabilities.surfaceCapabilities.maxImageCount)
    {
        imageCount = details.capabilities.surfaceCapabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.surface = physicalDevice.GetSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;

    PhysicalDevice::QueueFamilyData indices = physicalDevice.FindQueueFamilies(physicalDevice.GetVkPhysicalDevice());
    u32                             queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if(indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else { createInfo.imageSharingMode = vk::SharingMode::eExclusive; }

    createInfo.preTransform = details.capabilities.surfaceCapabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = oldSwap == nullptr ? VK_NULL_HANDLE : *oldSwap;

    if(m_logicalDevice.GetVkDevice().createSwapchainKHR(&createInfo, nullptr, &m_swapChain) != vk::Result::eSuccess)
    {
        HGFATAL("Failed to create swapchain!");
    }
    else
    {
        if(oldSwap != nullptr) { m_logicalDevice.GetVkDevice().destroySwapchainKHR(*oldSwap, nullptr); }
        m_surfaceFormat = surfaceFormat.surfaceFormat.format;
        m_extent = extent;
    }

    HGINFO("Created SwapChain");

    if(m_logicalDevice.GetVkDevice().getSwapchainImagesKHR(m_swapChain, &imageCount, nullptr) != vk::Result::eSuccess)
    {
        // throw error
    }
    m_images.resize(imageCount);
    if(m_logicalDevice.GetVkDevice().getSwapchainImagesKHR(m_swapChain, &imageCount, m_images.data()) != vk::Result::eSuccess)
    {
        // throw error
    }
    HGINFO("Got %d swapchain images", imageCount);
}

void SwapChain::CreateImageViews()
{
    m_imageViews.resize(m_images.size());

    for(size_t i = 0; i < m_imageViews.size(); i++)
    {
        vk::ImageViewCreateInfo createInfo{};
        createInfo.image = m_images[i];
        createInfo.viewType = vk::ImageViewType::e2D;
        createInfo.format = m_surfaceFormat;

        createInfo.components.r = vk::ComponentSwizzle::eIdentity;
        createInfo.components.g = vk::ComponentSwizzle::eIdentity;
        createInfo.components.b = vk::ComponentSwizzle::eIdentity;
        createInfo.components.a = vk::ComponentSwizzle::eIdentity;

        createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if(m_logicalDevice.GetVkDevice().createImageView(&createInfo, nullptr, &m_imageViews[i]) != vk::Result::eSuccess)
        {
            HGERROR("Failed to create image view!");
        }
    }
}

vk::SurfaceFormat2KHR SwapChain::ChooseSurfaceFormat(const std::vector<vk::SurfaceFormat2KHR>& formats)
{
    vk::SurfaceFormat2KHR comp{};
    comp.surfaceFormat = vk::Format::eR16G16B16A16Sfloat;

    for(const auto& format: formats)
    {
        if(format.surfaceFormat.format == comp.surfaceFormat.format && format.surfaceFormat.colorSpace == comp.surfaceFormat.colorSpace)
        {
            return format;
        }
    }

    // TODO: some sort of error handling
    return formats[0];
}

vk::PresentModeKHR SwapChain::ChoosePresentMode(const std::vector<vk::PresentModeKHR>& presentModes)
{
    for(const auto& mode: presentModes)
    {
        if(mode == vk::PresentModeKHR::eMailbox) { return mode; }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D SwapChain::ChooseExtent(const vk::SurfaceCapabilities2KHR& capabilities, Window& window)
{
    if(capabilities.surfaceCapabilities.currentExtent.width != std::numeric_limits<u32>::max())
    {
        return capabilities.surfaceCapabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window.GetWindow(), &width, &height);

        vk::Extent2D actualExtent = {static_cast<u32>(width), static_cast<u32>(height)};

        actualExtent.width = std::clamp(actualExtent.width, capabilities.surfaceCapabilities.minImageExtent.width,
                                        capabilities.surfaceCapabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.surfaceCapabilities.minImageExtent.height,
                                         capabilities.surfaceCapabilities.maxImageExtent.height);

        return actualExtent;
    }
}
} // namespace Humongous
