#pragma once

#include <gameobject.hpp>
#include <memory>
#include <render_pipeline.hpp>

namespace Humongous
{
class SimpleRenderSystem
{
public:
    SimpleRenderSystem(LogicalDevice& logicalDevice);
    ~SimpleRenderSystem();

    void RenderObjects(GameObject::Map& gameObjects, VkCommandBuffer commandBuffer);

private:
    LogicalDevice&                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    VkPipelineLayout                m_pipelineLayout;

    void CreatePipelineLayout();
    void CreatePipeline();
};
} // namespace Humongous
