#pragma once

#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool.hpp"
#include "camera.hpp"
#include "defines.hpp"
#include "extra.hpp"
#include "images.hpp"
#include "logical_device.hpp"
#include "swapchain.hpp"
#include "vk_mem_alloc.h"

#include <memory>
#include <vector>

namespace Humongous
{
class Renderer
{
private:
    struct VisiblityResultSet
    {
        n32 id;
        n32 visible;
    };
    // static_assert(sizeof(VisiblityResultSet) == 8, "Must be 8 bytes to match GLSL std430");

    struct GBuffer
    {
        vk::DescriptorSet imageSet;

        AllocatedImage albedo;
        AllocatedImage normalRough;
        AllocatedImage materialParam;
        AllocatedImage position;
        AllocatedImage depth;
    };

public:
    struct Frame
    {
        vk::CommandBuffer commandBuffer;
        vk::Semaphore     imageAvailableSemaphore;
        vk::Fence         inFlightFence;

        GBuffer gbuffer;

        std::unique_ptr<Buffer> objectDataBuffer;
        std::unique_ptr<Buffer> visiblityResultBuffer;
        std::unique_ptr<Buffer> rendererDataBuffer;
        std::unique_ptr<Buffer> debugBuffer;

        std::vector<VisiblityResultSet> visiblityResults;
        n32                             numObjectsDispatched;

        AllocatedImage drawImage;

        struct DepthMip
        {
            vk::ImageView     sampledView;
            vk::ImageView     storageView;
            vk::DescriptorSet set;
            vk::ImageLayout   layout;
        };
        AllocatedImage        hiZImage;
        std::vector<DepthMip> hiZMips;

        vk::DescriptorSet occlusionSet;
        n32               boundingBoxCount;
    };

    Renderer(Window& window, LogicalDevice& logicalDevice, PhysicalDevice& physicalDevice, VmaAllocator allocator, vk::Format drawFormat,
             vk::Format depthFormat);
    ~Renderer();

    // Get the swapchain image index we're currently using
    n32 GetImageIndex() const { return m_currentImageIndex; }

    // Get the frame index we're currently using
    n32 GetFrameIndex() const { return m_currentFrameIndex; }

    // Get the command buffer we're currently using
    vk::CommandBuffer GetCommandBuffer() { return GetCurrentFrame().commandBuffer; }

    // Begin a frame, acquire the next swapchain image and prep command buffers
    vk::CommandBuffer BeginFrame(std::vector<Utils::VisibleEntityInfo>& visibleEntities);

    void ReadyPerFrameData(std::vector<Utils::VisibleEntityInfo>& visibleEntities);

    // End a frame and submit command buffers
    void EndFrame();

    void BeginDepthPrePass(vk::CommandBuffer cmd);
    void EndDepthPrePass(vk::CommandBuffer cmd);

    // Get the swapchain's aspect ratio
    f32 GetAspectRatio() const { return static_cast<float>(m_swapChain->GetExtent().width) / static_cast<float>(m_swapChain->GetExtent().height); }

    /***
     * Begin listening for draw commands.
     *
     * commandBuffer: the command buffer we'll write the commands to
     *
     */
    void BeginGeometryPass(vk::CommandBuffer commandBuffer);

    /***
     *  Stop listening for draw commands and copy the outputs to the final swapchain image
     */
    void EndGeometryPass(vk::CommandBuffer commandBuffer);

    void DoLightingPass(vk::CommandBuffer cmd, vk::DescriptorSet camSet, vk::DescriptorSet sceneSet, vk::DescriptorSet skyboxSet);

    void BeginUIRendering(vk::CommandBuffer cmd);
    void EndUIRendering(vk::CommandBuffer cmd);

    void BeginSkyboxPass(vk::CommandBuffer cmd);
    void EndSkyboxPass(vk::CommandBuffer cmd);

    SwapChain* GetSwapChain() const { return m_swapChain.get(); }

    void DoOcclusionCulling(vk::CommandBuffer cmd, const std::vector<struct Utils::VisibleEntityInfo>& frustumCulledEntities, class World& world,
                            const Camera& cam);

    static void WaitForCompute(vk::CommandBuffer cmd);

private:
    std::unique_ptr<SwapChain> m_swapChain = nullptr;
    Window&                    m_window;
    LogicalDevice&             m_logicalDevice;
    PhysicalDevice&            m_physicalDevice;

    vk::Pipeline       m_occlusionPipeline;
    vk::PipelineLayout m_occlusionPipelineLayout;
    vk::Pipeline       m_mipPipeline;
    vk::PipelineLayout m_mipPipelineLayout;

    std::unique_ptr<DescriptorPool>      m_computePool;
    std::unique_ptr<DescriptorSetLayout> m_occlusionDescriptorLayout;
    std::unique_ptr<DescriptorSetLayout> m_mipDescriptorLayout;

    vk::Pipeline                         m_lightingPipeline;
    vk::PipelineLayout                   m_lightingPipelineLayout;
    std::unique_ptr<DescriptorSetLayout> m_lightingDescriptorLayout;
    std::unique_ptr<DescriptorPool>      m_lightingPool;

    VmaAllocator m_allocator;

    vk::CommandPool    m_commandPool;
    std::vector<Frame> m_frames;

    n32    m_currentImageIndex{0};
    n32    m_currentFrameIndex{0};
    Frame& GetCurrentFrame() { return m_frames[m_currentFrameIndex]; }

    vk::Extent2D m_screenImageExtent;

    vk::Sampler    m_depthImageSampler;
    AllocatedImage m_debugImage;
    vk::Sampler    m_debugImageSampler;

    void PreGeometryPassTransitions(vk::CommandBuffer cmd);
    void PostGeometryPassTransitions(vk::CommandBuffer cmd);

    void CreateGBuffer();
    void CreateLightingPipeline();
    void CreateDrawImage();
    void CreateDepthImage();
    void CreateSyncStructures();
    void CreateComputePipeline();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void RecreateSwapChain();
    void UpdateDepthBuffer();
};
} // namespace Humongous
