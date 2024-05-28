#pragma once

#include "defines.hpp"
#include <logical_device.hpp>
#include <memory>
#include <swapchain.hpp>

#include <images.hpp>

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

    // Set depthFormat to VK_FORMAT_UNDEFINED to not have depth
    Renderer(Window& window, LogicalDevice& logicalDevice, PhysicalDevice& physicalDevice, VmaAllocator allocator, VkFormat drawFormat,
             VkFormat depthFormat);
    ~Renderer();

    // Get the swapchain image index we're currently using
    u32 GetImageIndex() const { return m_currentImageIndex; }

    // Get the frame index we're currently using
    u32 GetFrameIndex() const { return m_currentFrameIndex; }

    // Get the command buffer we're currently using
    VkCommandBuffer GetCommandBuffer() { return GetCurrentFrame().commandBuffer; }

    // Begin a frame, acquire the next swapchain image and prep command buffers
    VkCommandBuffer BeginFrame();

    // End a frame and submit command buffers
    void EndFrame();

    // Get the swapchain's aspect ratio
    f32 GetAspectRatio() const { return static_cast<float>(m_swapChain->GetExtent().width) / static_cast<float>(m_swapChain->GetExtent().height); }

    /***
     * Begin listening for draw commands.
     *
     * commandBuffer: the command buffer we'll write the commands to
     *
     */
    void BeginRendering(VkCommandBuffer commandBuffer);

    /***
     *  Stop listening for draw commands and copy the outputs to the final swapchain image
     */
    void EndRendering(VkCommandBuffer commandBuffer);

    SwapChain* GetSwapChain() const { return m_swapChain.get(); }

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
