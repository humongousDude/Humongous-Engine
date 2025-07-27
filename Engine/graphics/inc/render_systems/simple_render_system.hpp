#pragma once

#include "camera.hpp"
#include "extra.hpp"
#include "render_pipeline.hpp"
#include "world.hpp"

#include <memory>

namespace Humongous
{
struct RenderData
{
    vk::CommandBuffer                            commandBuffer;
    std::vector<vk::DescriptorSet>               uboSets;
    const std::vector<Utils::VisibleEntityInfo>* visibleEntities;
    Humongous::World&                            world;
    n32                                          frameIndex;
    Camera&                                      cam;
};

struct ShaderSet
{
    std::string vertShaderPath;
    std::string fragShaderPath;
};

class SimpleRenderSystem
{
public:
    SimpleRenderSystem(const LogicalDevice& logicalDevice, ResourceManager& resourceManager,
                       const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts, const ShaderSet& shaderSet);
    ~SimpleRenderSystem();

    void RenderObjectsMesh(RenderData& renderData, const bool& depthOnly);
    void RenderObjects(RenderData& renderData, const bool& depthOnly);
    n32  GetObjectsDrawn() { return m_verticesDrawn; }

    vk::PipelineLayout m_pipelineLayout{};

private:
    struct alignas(16) DrawData
    {
        n32 materialID{0};
        n32 localNodeIndex{0};
        n32 isSkinned{0};
        n32 isMorphed{0};
        n32 instanceOffset;
    };

    struct alignas(16) InstanceData
    {
        Eigen::Matrix4f modelMatrix;
        n32             modelID;
        n32             globalNodeIndex;
        n32             jointMatrixStart;
        n32             morphTargetStart;
    };

    const LogicalDevice&            m_logicalDevice;
    ResourceManager&                m_resourceManager;
    std::unique_ptr<RenderPipeline> m_opaqueGeometryPipeline;
    std::unique_ptr<RenderPipeline> m_transparentGeometryPipeline;
    std::unique_ptr<RenderPipeline> m_depthOnlyPipeline;
    n32                             m_verticesDrawn{0};

    std::unique_ptr<Buffer>              m_debugBuffer;
    std::unique_ptr<DescriptorSetLayout> m_layout;

    std::vector<std::unique_ptr<DescriptorPool>> m_pool;
    std::vector<std::unique_ptr<DescriptorPool>> m_depthPool;

    std::vector<std::unique_ptr<Buffer>> m_indirectDrawBuffers;

    std::vector<std::unique_ptr<Buffer>> m_drawDataBuffers;
    std::vector<std::unique_ptr<Buffer>> m_drawInstanceBuffers;
    std::vector<vk::DescriptorSet>       m_set = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

    std::vector<std::unique_ptr<Buffer>> m_depthIndirectDrawBuffers;
    std::vector<std::unique_ptr<Buffer>> m_depthDrawDataBuffers;
    std::vector<std::unique_ptr<Buffer>> m_depthInstanceBuffers;
    std::vector<vk::DescriptorSet>       m_depthSet = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

    void AllocateDescriptorSet();
    void CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts);
    void CreatePipeline(const ShaderSet& shaderSet);

    void RenderObjectsToData(RenderData& renderData, std::vector<DrawData>& opaqueDrawData,
                             std::vector<vk::DrawIndexedIndirectCommand>& opaqueCommands, std::vector<DrawData>& transparentDrawData,
                             std::vector<vk::DrawIndexedIndirectCommand>& transparentCommands, std::vector<InstanceData>& instanceData);
};
} // namespace Humongous
