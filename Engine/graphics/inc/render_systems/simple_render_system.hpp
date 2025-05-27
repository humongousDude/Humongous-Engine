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
    std::vector<vk::DescriptorSet> uboSets;    // Camera UBOs
    std::vector<vk::DescriptorSet> sceneSets;  // Scene-wide parameters
    std::vector<vk::DescriptorSet> skyboxSets; // Skybox specific (might not be used by object rendering)

    // List of entities to render in this pass
    const std::vector<Utils::VisibleEntityInfo>* visibleEntities;

    Humongous::World& world; // Reference to the ECS World

    n32     frameIndex;
    Camera& cam;
    // const glm::vec3 camPos; // Can get from cam.GetPosition() if needed
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
    // void DepthOnlyRender(RenderData& renderData);
    n32 GetObjectsDrawn() { return m_verticesDrawn; }

    vk::PipelineLayout m_pipelineLayout{};

private:
    LogicalDevice&                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    std::unique_ptr<RenderPipeline> m_depthOnlyPipeline;
    n32                             m_verticesDrawn{0};

    std::unique_ptr<Buffer> m_debugBuffer;

    void CreateModelDescriptorSetPool();
    void CreateModelDescriptorSetLayout();
    void AllocateDescriptorSet(n32 identifier, n32 index);
    void CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts);
    void CreatePipeline(const ShaderSet& shaderSet);
};
} // namespace Humongous
