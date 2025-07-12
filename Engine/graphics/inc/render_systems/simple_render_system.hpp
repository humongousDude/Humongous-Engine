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
    vk::CommandBuffer              commandBuffer;
    std::vector<vk::DescriptorSet> uboSets; // Camera UBOs

    // Since DoLightingPass is called from the renderer, these aren't needed here. Though I'll keep them just in case
    // std::vector<vk::DescriptorSet> sceneSets;  // Scene-wide parameters
    // std::vector<vk::DescriptorSet> skyboxSets; // Skybox specific (might not be used by object rendering)

    const std::vector<Utils::VisibleEntityInfo>* visibleEntities;

    Humongous::World& world;
    n32               frameIndex;
    Camera&           cam;
};

struct ShaderSet
{
    std::string vertShaderPath;
    std::string fragShaderPath;
};

class SimpleRenderSystem
{
public:
    SimpleRenderSystem(LogicalDevice& logicalDevice, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts, const ShaderSet& shaderSet);
    ~SimpleRenderSystem();

    void RenderObjects(RenderData& renderData, const bool& depthOnly);
    n32  GetObjectsDrawn() { return m_verticesDrawn; }

    vk::PipelineLayout m_pipelineLayout{};

private:
    LogicalDevice&                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_geometryPipeline;
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
};
} // namespace Humongous
