#pragma once

#include "defines.hpp"
#include <logical_device.hpp>
#include <memory>
#include <swapchain.hpp>

#include <vk_mem_alloc.h>

namespace Humongous
{
class Renderer
{
public:
    struct Frame
    {
        VkCommandBuffer commandBuffer;
        VkSemaphore     imageAvailableSemaphore;
        VkSemaphore     renderFinishedSemaphore;
        VkFence         inFlightFence;
    };

    struct AllocatedImage
    {
        VkImage       image{VK_NULL_HANDLE};
        VkImageView   imageView{VK_NULL_HANDLE};
        VmaAllocation allocation;
        VkExtent3D    imageExtent;
        VkFormat      imageFormat;
    };

    Renderer(Window& window, LogicalDevice& logicalDevice, PhysicalDevice& physicalDevice, VmaAllocator allocator);
    ~Renderer();

    u32             GetImageIndex() const { return m_currentImageIndex; }
    u32             GetFrameIndex() const { return m_currentFrameIndex; }
    VkCommandBuffer GetCommandBuffer() { return GetCurrentFrame().commandBuffer; }

    VkCommandBuffer BeginFrame();
    void            EndFrame();

    f32 GetAspectRatio() const { return static_cast<float>(m_swapChain->GetExtent().width) / static_cast<float>(m_swapChain->GetExtent().height); }

    void BeginRendering(VkCommandBuffer commandBuffer);
    void EndRendering(VkCommandBuffer commandBuffer);

private:
    std::unique_ptr<SwapChain> m_swapChain = nullptr;
    Window&                    m_window;
    LogicalDevice&             m_logicalDevice;
    PhysicalDevice&            m_physicalDevice;

    VmaAllocator m_allocator;

    VkCommandPool      m_commandPool;
    std::vector<Frame> m_frames;

    u32    m_currentImageIndex;
    int    m_currentFrameIndex{0};
    Frame& GetCurrentFrame() { return m_frames[m_currentFrameIndex]; }

    AllocatedImage m_drawImage;
    VkExtent2D     m_drawImageExtent;
    AllocatedImage m_depthImage;
    VkExtent2D     m_depthImageExtent;

    void InitImagesAndViews();
    void InitDepthImage();
    void InitSyncStructures();
    void CreateCommandPool();
    void AllocateCommandBuffers();
    void RecreateSwapChain();
};
} // namespace Humongous
