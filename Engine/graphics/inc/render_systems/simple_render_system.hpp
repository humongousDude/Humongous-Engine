#pragma once

#include "camera.hpp"
#include <gameobject.hpp>
#include <memory>
#include <render_pipeline.hpp>

namespace Humongous
{
struct RenderData
{
    VkCommandBuffer                                       commandBuffer;
    std::vector<VkDescriptorSet>                          uboSets;
    std::vector<VkDescriptorSet>                          sceneSets;
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
    SimpleRenderSystem(LogicalDevice& logicalDevice, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, const ShaderSet& shaderSet);
    ~SimpleRenderSystem();

    void RenderObjects(RenderData& renderData);
    void DepthOnlyRender(RenderData& renderData);
    s16  GetObjectsDrawn() { return m_verticesDrawn; }

private:
    LogicalDevice&                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    std::unique_ptr<RenderPipeline> m_depthOnlyPipeline;
    VkPipelineLayout                m_pipelineLayout{};
    n32                             m_verticesDrawn{0};

    std::unique_ptr<Buffer> m_debugBuffer;

    void CreateModelDescriptorSetPool();
    void CreateModelDescriptorSetLayout();
    void AllocateDescriptorSet(n32 identifier, n32 index);
    void CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
    void CreatePipeline(const ShaderSet& shaderSet);
};
} // namespace Humongous
