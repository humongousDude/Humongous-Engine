#pragma once

#include "camera.hpp"
#include <gameobject.hpp>
#include <memory>
#include <render_pipeline.hpp>

namespace Humongous
{
struct RenderData
{
    vk::CommandBuffer                                     commandBuffer;
    std::vector<vk::DescriptorSet>                        uboSets;
    std::vector<vk::DescriptorSet>                        sceneSets;
    std::vector<vk::DescriptorSet>                        skyboxSets;
    std::vector<std::pair<GameObject::id_t, GameObject*>> gameObjects;
    n32                                                   frameIndex;
    Camera&                                               cam;
    const Renderer&                                       renderer;
    const glm::vec3                                       camPos;
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

private:
    LogicalDevice&                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    std::unique_ptr<RenderPipeline> m_depthOnlyPipeline;
    vk::PipelineLayout              m_pipelineLayout{};
    n32                             m_verticesDrawn{0};

    std::unique_ptr<Buffer> m_debugBuffer;

    void CreateModelDescriptorSetPool();
    void CreateModelDescriptorSetLayout();
    void AllocateDescriptorSet(n32 identifier, n32 index);
    void CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts);
    void CreatePipeline(const ShaderSet& shaderSet);
};
} // namespace Humongous
