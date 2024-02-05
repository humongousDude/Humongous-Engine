#pragma once

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
        VkImage       image;
        VkImageView   imageView;
        VmaAllocation allocation;
        VkExtent3D    imageExtent;
        VkFormat      imageFormat;
    };

    Renderer(Window& window, LogicalDevice& logicalDevice, PhysicalDevice& physicalDevice, VmaAllocator& allocator);
    ~Renderer();

    VkCommandBuffer GetCommandBuffer() { return GetCurrentFrame().commandBuffer; }

    VkCommandBuffer BeginFrame();
    void            EndFrame();

    void BeginRendering(VkCommandBuffer commandBuffer);
    void EndRendering(VkCommandBuffer commandBuffer);

private:
    std::unique_ptr<SwapChain> m_swapChain;
    Window&                    m_window;
    LogicalDevice&             m_logicalDevice;
    PhysicalDevice&            m_physicalDevice;

    VmaAllocator& m_allocator;

    VkCommandPool      m_commandPool;
    std::vector<Frame> m_frames;

    u32    m_currentImageIndex;
    int    m_currentFrameIndex{0};
    Frame& GetCurrentFrame() { return m_frames[m_currentFrameIndex % SwapChain::MAX_FRAMES_IN_FLIGHT]; }

    AllocatedImage m_drawImage;
    VkExtent2D     m_drawImageExtent;

    void InitImagesAndViews();
    void InitSyncStructures();
    void CreateCommandPool();
    void AllocateCommandBuffers();
};
} // namespace Humongous
