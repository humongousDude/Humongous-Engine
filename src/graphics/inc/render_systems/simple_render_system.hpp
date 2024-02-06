#pragma once

#include "render_pipeline.hpp"
#include <memory>

namespace Humongous
{
class SimpleRenderSystem
{
public:
    SimpleRenderSystem(LogicalDevice& logicalDevice);
    ~SimpleRenderSystem();

    // TODO: add game objects class
    // void RenderObjects(std::vector<GameObject>& objects);
private:
    LogicalDevice& m_logicalDevice;

    std::unique_ptr<RenderPipeline> m_renderPipeline;
    VkPipelineLayout                m_pipelineLayout;
};
} // namespace Humongous
