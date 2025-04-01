#pragma once

#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool.hpp"
#include "camera.hpp"
#include "defines.hpp"
#include "render_pipeline.hpp"
#include <images.hpp>
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
        vk::CommandBuffer commandBuffer;
        vk::Semaphore     imageAvailableSemaphore;
        vk::Semaphore     renderFinishedSemaphore;
        vk::Fence         inFlightFence;
    };

    // Set depthFormat to VK_FORMAT_UNDEFINED to not have depth
    Renderer(Window& window, LogicalDevice& logicalDevice, PhysicalDevice& physicalDevice, VmaAllocator allocator, VkFormat drawFormat,
             VkFormat depthFormat);
    ~Renderer();

    // Get the swapchain image index we're currently using
    n32 GetImageIndex() const { return m_currentImageIndex; }

    // Get the frame index we're currently using
    n32 GetFrameIndex() const { return m_currentFrameIndex; }

    // Get the command buffer we're currently using
    VkCommandBuffer GetCommandBuffer() { return GetCurrentFrame().commandBuffer; }

    // Begin a frame, acquire the next swapchain image and prep command buffers
    VkCommandBuffer BeginFrame();

    // End a frame and submit command buffers
    void EndFrame();

    void BeginDepthPrePass(VkCommandBuffer cmd);
    void EndDepthPrePass(VkCommandBuffer cmd);

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

    void DoGPUOcclusionCulling(VkCommandBuffer cmd, struct RenderData& objs, const Camera& cam);

    VkDescriptorBufferInfo VisibilityResultDescriptorData() const
    {
        if(m_visibilityResults) { return m_visibilityResults->DescriptorInfo(); }
        else { return {}; }
    }

private:
    std::unique_ptr<SwapChain> m_swapChain = nullptr;
    Window&                    m_window;
    LogicalDevice&             m_logicalDevice;
    PhysicalDevice&            m_physicalDevice;

    VkPipeline       m_computePipeline;
    VkPipelineLayout m_computePipelineLayout;

    std::unique_ptr<DescriptorPool>      m_computePool;
    std::unique_ptr<DescriptorSetLayout> m_computeLayout;
    VkDescriptorSet                      m_computeSet{VK_NULL_HANDLE};

    VmaAllocator m_allocator;

    vk::CommandPool    m_commandPool;
    std::vector<Frame> m_frames;

    n32    m_currentImageIndex{0};
    n32    m_currentFrameIndex{0};
    Frame& GetCurrentFrame() { return m_frames[m_currentFrameIndex]; }

    AllocatedImage m_drawImage;
    vk::Extent2D   m_drawImageExtent;

    AllocatedImage m_depthImage;
    vk::Extent2D   m_depthImageExtent;
    VkSampler      m_depthImageSampler;

    std::unique_ptr<Buffer> m_boundingBoxBuffer;
    std::unique_ptr<Buffer> m_visibilityResults;
    std::unique_ptr<Buffer> m_rendererDataBuffer;
    std::unique_ptr<Buffer> m_debugBuffer;

    AllocatedImage m_debugImage;
    VkSampler      m_debugImageSampler;

    void InitImagesAndViews();
    void InitDepthImage();
    void InitSyncStructures();
    void CreateComputePipeline();
    void CreateCommandPool();
    void AllocateCommandBuffers();
    void RecreateSwapChain();
    void UpdateDepthBuffer();

    void WaitForCompute(VkCommandBuffer cmd);
};
} // namespace Humongous
