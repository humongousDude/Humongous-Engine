#pragma once

#include "camera.hpp"
#include "extra.hpp"
#include "render_pipeline.hpp"
#include "swapchain.hpp"
#include "world.hpp"

#include <memory>

namespace Humongous
{
struct RenderData
{
    vk::CommandBuffer                            commandBuffer;
    std::vector<vk::DescriptorSet>               uboSets;
    const std::vector<Utils::VisibleEntityInfo>* entities;
    Humongous::World&                            world;
    u32                                          frameIndex;
    Camera&                                      cam;
};

class IRenderSystem
{
public:
    struct alignas(16) DrawData
    {
        u32 vertexOffset{0};
        u32 materialID{0};
        u32 localNodeIndex{0};
        u32 isSkinned{0};
        u32 isMorphed{0};
        u32 instanceOffset{0};
        u32 pad[2];
    };

    struct alignas(16) InstanceData
    {
        Eigen::Matrix4f modelMatrix;
        u32             modelID;
        u32             globalNodeIndex;
        u32             jointMatrixStart;
        u32             morphTargetStart;
    };

    IRenderSystem(const ILogicalDevice& logicalDevice, ResourceManager& resourceManager, const IAssetManager& assetManager,
                  const RenderPipeline::PipelineConfigInfo& configInfo)
        : m_logicalDevice{logicalDevice}, m_resourceManager{resourceManager}, m_assetManager{assetManager}
    {
        CreatePipeline(configInfo);
        AllocateDescriptorSet();
    }
    virtual ~IRenderSystem();

    /**
     * @brief Render the given objects contained within renderData. Must be implemented by the derived class.
     *
     * @param renderData: The data needed to render the objects
     */
    virtual void Render(RenderData& renderData) = 0;
    virtual void ReadyBuffers(RenderData& renderData) = 0;
    virtual void ReadyDescriptors(RenderData& renderData) = 0;

    void RenderObjectsToData(RenderData& renderData, std::vector<DrawData>& opaqueDrawData,
                             std::vector<vk::DrawIndexedIndirectCommand>& opaqueCommands, std::vector<DrawData>& transparentDrawData,
                             std::vector<vk::DrawIndexedIndirectCommand>& transparentCommands, std::vector<InstanceData>& instanceData);

protected:
    /**
     * @brief Create the render pipeline.
     *
     * @param configInfo: The configuration info for the pipeline
     */
    virtual void CreatePipeline(const RenderPipeline::PipelineConfigInfo& configInfo);
    virtual void AllocateDescriptorSet();

    const ILogicalDevice&           m_logicalDevice;
    ResourceManager&                m_resourceManager;
    const IAssetManager&            m_assetManager;
    std::unique_ptr<RenderPipeline> m_pipeline;

    std::array<std::unique_ptr<DescriptorPool>, SwapChain::MAX_FRAMES_IN_FLIGHT> m_pool{};
    std::array<vk::DescriptorSet, SwapChain::MAX_FRAMES_IN_FLIGHT>               m_set{};
};

class MeshRenderSystem : public IRenderSystem
{
public:
    struct MeshletDrawInfo
    {
        u32 meshletOffset;
        u32 drawDataIndex;
        u32 instanceOffset;
        u32 instanceCount;
        u32 meshletCount;
    };

    MeshRenderSystem(const ILogicalDevice& logicalDevice, ResourceManager& resourceManager, const IAssetManager& assetManager,
                     const RenderPipeline::PipelineConfigInfo& configInfo);
    ~MeshRenderSystem();

    void ReadyBuffers(RenderData& renderData) override;
    void Render(RenderData& renderData) override;
    void ReadyDescriptors(RenderData& renderData) override;

private:
    std::array<u32, SwapChain::MAX_FRAMES_IN_FLIGHT> m_drawCount{};

    std::array<std::unique_ptr<Buffer>, SwapChain::MAX_FRAMES_IN_FLIGHT> m_drawDataBuffers{};
    std::array<std::unique_ptr<Buffer>, SwapChain::MAX_FRAMES_IN_FLIGHT> m_instanceBuffers{};
    std::array<std::unique_ptr<Buffer>, SwapChain::MAX_FRAMES_IN_FLIGHT> m_meshDataBuffers{};
    std::array<std::unique_ptr<Buffer>, SwapChain::MAX_FRAMES_IN_FLIGHT> m_indirectBuffers{};
    std::array<std::unique_ptr<Buffer>, SwapChain::MAX_FRAMES_IN_FLIGHT> m_shaderbufferVisibleIndices{};
    std::array<std::unique_ptr<Buffer>, SwapChain::MAX_FRAMES_IN_FLIGHT> m_shaderbufferVisibleCounter{};
};

class TraditionalRenderSystem : public IRenderSystem
{
public:
    TraditionalRenderSystem(const ILogicalDevice& logicalDevice, ResourceManager& resourceManager, const IAssetManager& assetManager,
                            const RenderPipeline::PipelineConfigInfo& configInfo);
    ~TraditionalRenderSystem();

    void Render(RenderData& renderData) override;
    void ReadyBuffers(RenderData& renderData) override {};
    void ReadyDescriptors(RenderData& renderData) override {};

private:
    std::unique_ptr<Buffer> m_debugBuffer{};

    std::array<std::unique_ptr<Buffer>, SwapChain::MAX_FRAMES_IN_FLIGHT> m_indirectDrawBuffers{};
    std::array<std::unique_ptr<Buffer>, SwapChain::MAX_FRAMES_IN_FLIGHT> m_drawDataBuffers{};
    std::array<std::unique_ptr<Buffer>, SwapChain::MAX_FRAMES_IN_FLIGHT> m_drawInstanceBuffers{};
};
} // namespace Humongous
