#pragma once

#include "render_pipeline.hpp"
#include "window.hpp"
#include <deque>
#include <functional>
#include <instance.hpp>
#include <logical_device.hpp>
#include <memory>
#include <physical_device.hpp>
#include <swapchain.hpp>
#include <vk_mem_alloc.h>

namespace Humongous
{
struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    void PushDeletor(std::function<void()> deletor) { deletors.push_front(deletor); }

    void Flush()
    {
        for(auto& deletor: deletors) { deletor(); }
        deletors.clear();
    }
};

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

// TODO: Refactor
class VulkanApp
{
public:
    VulkanApp();
    ~VulkanApp();

    void Run();

private:
    DeletionQueue m_mainDeletionQueue;

    std::unique_ptr<Instance>       m_instance;
    std::unique_ptr<Window>         m_window;
    std::unique_ptr<PhysicalDevice> m_physicalDevice;
    std::unique_ptr<LogicalDevice>  m_logicalDevice;
    std::unique_ptr<SwapChain>      m_swapChain;
    std::unique_ptr<RenderPipeline> m_renderPipeline;

    VkPipelineLayout pipelineLayout;

    VkCommandPool      m_commandPool;
    std::vector<Frame> m_frames;

    int    frameNumber{0};
    Frame& GetCurrentFrame() { return m_frames[frameNumber % SwapChain::MAX_FRAMES_IN_FLIGHT]; }

    VmaAllocator m_allocator;

    AllocatedImage m_drawImage;
    VkExtent2D     m_drawImageExtent;

    void Init();
    // TODO: move this function
    void CreatePipelineLayout();
    void InitSyncStructures();
    void CreateCommandPool();
    void AllocateCommandBuffers();
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, u32 imageIndex);
    void SubmitCommandBuffer(VkCommandBuffer commandBuffer, u32 imageIndex);
    void DrawFrame();
};
} // namespace Humongous
