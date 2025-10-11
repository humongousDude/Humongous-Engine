#pragma once

#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool.hpp"
#include "abstractions/image.hpp"
#include "camera.hpp"
#include "compute_pipeline.hpp"
#include "defines.hpp"
#include "extra.hpp"
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
        u32 id;
        u32 visible;
    };

    struct GBuffer
    {
        vk::DescriptorSet imageSet;

        std::unique_ptr<Image> albedo;
        std::unique_ptr<Image> normalRough;
        std::unique_ptr<Image> materialParam;
        std::unique_ptr<Image> depth;
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
        u32                             numObjectsDispatched;

        std::unique_ptr<Image> drawImage;

        struct DepthMip
        {
            vk::ImageView     sampledView;
            vk::ImageView     storageView;
            vk::DescriptorSet set;
            vk::ImageLayout   layout;
        };
        std::unique_ptr<Image> hiZImage;
        std::vector<DepthMip>  hiZMips;

        vk::DescriptorSet occlusionSet;
        u32               boundingBoxCount;

        std::vector<WorkScheduler::WorkPacketHandle> workPackets;
        b8                                           started{false};
    };

    Renderer(Window& window, const ILogicalDevice& logicalDevice, const PhysicalDevice& physicalDevice, ResourceManager& resourceManager,
             const IAssetManager& assetManager);
    ~Renderer();

    // Get the swapchain image index we're currently using
    u32 GetImageIndex() const { return m_currentImageIndex; }

    // Get the frame index we're currently using
    u32 GetFrameIndex() const { return m_currentFrameIndex; }

    // Get the command buffer we're currently using
    vk::CommandBuffer GetCommandBuffer() { return GetCurrentFrame().commandBuffer; }

    // Begin a frame, acquire the next swapchain image and wait on the Frames's fences and semaphores, returns the command buffer that was started
    vk::CommandBuffer BeginFrame(std::vector<Utils::VisibleEntityInfo>& visibleEntities);

    // Ready the per frame data, like the visible entities. I'd like to remove the need for this function entirely if possible or feasible
    // called in BeginFrame();
    void ReadyPerFrameData(std::vector<Utils::VisibleEntityInfo>& visibleEntities);

    // End a frame and submit command buffers
    void EndFrame();

    void BeginDepthPrePass(vk::CommandBuffer cmd);
    void EndDepthPrePass(vk::CommandBuffer cmd);

    // Get the swapchain's aspect ratio
    f32 GetAspectRatio() const { return static_cast<float>(m_swapChain->GetExtent().width) / static_cast<float>(m_swapChain->GetExtent().height); }

    void BeginGeometryPass(vk::CommandBuffer commandBuffer);
    void EndGeometryPass(vk::CommandBuffer commandBuffer);

    void DoLightingPass(vk::CommandBuffer cmd, vk::DescriptorSet camSet, vk::DescriptorSet sceneSet, vk::DescriptorSet skyboxSet);

    void BeginUIPass(vk::CommandBuffer cmd);
    void EndUIPass(vk::CommandBuffer cmd);

    void BeginSkyboxPass(vk::CommandBuffer cmd);
    void EndSkyboxPass(vk::CommandBuffer cmd);

    SwapChain* GetSwapChain() const { return m_swapChain.get(); }

    // Do occlusion culling on the visible entities, and write the results to the visibility results buffer to be retrieved and updated via
    // ReadyPerFrameData() the next frame
    void DoOcclusionCulling(vk::CommandBuffer cmd, const std::vector<struct Utils::VisibleEntityInfo>& frustumCulledEntities, class World& world,
                            const Camera& cam);

    static void WaitForCompute(vk::CommandBuffer cmd);

private:
    std::unique_ptr<SwapChain> m_swapChain = nullptr;
    Window&                    m_window;
    const ILogicalDevice&      m_logicalDevice;
    const IAssetManager&       m_assetManager;
    ResourceManager&           m_resourceManager;
    const PhysicalDevice&      m_physicalDevice;

    std::unique_ptr<ComputePipeline> m_occlusionPipeline;
    vk::PipelineLayout               m_occlusionPipelineLayout;
    std::unique_ptr<ComputePipeline> m_mipPipeline;
    vk::PipelineLayout               m_mipPipelineLayout;

    std::unique_ptr<DescriptorPool>      m_computePool;
    std::unique_ptr<DescriptorSetLayout> m_occlusionDescriptorLayout;
    std::unique_ptr<DescriptorSetLayout> m_mipDescriptorLayout;

    std::unique_ptr<ComputePipeline>     m_lightingPipeline;
    vk::PipelineLayout                   m_lightingPipelineLayout;
    std::unique_ptr<DescriptorSetLayout> m_lightingDescriptorLayout;
    std::unique_ptr<DescriptorPool>      m_lightingPool;

    vk::CommandPool    m_commandPool;
    std::vector<Frame> m_frames;

    u32    m_currentImageIndex{0};
    u32    m_currentFrameIndex{0};
    Frame& GetCurrentFrame() { return m_frames[m_currentFrameIndex]; }

    vk::Extent2D m_screenImageExtent;

    vk::Sampler m_depthImageSampler;

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
