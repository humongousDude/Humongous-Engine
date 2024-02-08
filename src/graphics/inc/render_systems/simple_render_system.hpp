#pragma once

#include <gameobject.hpp>
#include <memory>
#include <render_pipeline.hpp>

namespace Humongous
{
struct RenderData
{
    VkCommandBuffer  commandBuffer;
    VkDescriptorSet  globalSet;
    GameObject::Map& gameObjects;
};

class SimpleRenderSystem
{
public:
    SimpleRenderSystem(LogicalDevice& logicalDevice, VkDescriptorSetLayout globalLayout);
    ~SimpleRenderSystem();

    void RenderObjects(RenderData& renderData);

private:
    LogicalDevice&                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    VkPipelineLayout                m_pipelineLayout;

    void CreatePipelineLayout(VkDescriptorSetLayout globalLayout);
    void CreatePipeline();
};
} // namespace Humongous
