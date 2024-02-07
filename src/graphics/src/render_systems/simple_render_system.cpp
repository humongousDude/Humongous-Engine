#include "logger.hpp"
#include <render_systems/simple_render_system.hpp>

namespace Humongous
{
SimpleRenderSystem::SimpleRenderSystem(LogicalDevice& logicalDevice) : m_logicalDevice(logicalDevice)
{
    HGINFO("Creating simple render system...");
    CreatePipelineLayout();
    CreatePipeline();
    HGINFO("Created simple render system");
}

SimpleRenderSystem::~SimpleRenderSystem()
{
    HGINFO("Destroying simple render system...");
    vkDestroyPipelineLayout(m_logicalDevice.GetVkDevice(), m_pipelineLayout, nullptr);
    HGINFO("Destroyed Simple render system");
}

void SimpleRenderSystem::CreatePipelineLayout()
{
    HGINFO("Creating pipeline layout...");
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if(vkCreatePipelineLayout(m_logicalDevice.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create pipeline layout");
    }

    HGINFO("Created pipeline layout");
}

void SimpleRenderSystem::CreatePipeline()
{
    HGINFO("Creating pipeline...");
    RenderPipeline::PipelineConfigInfo configInfo = RenderPipeline::DefaultPipelineConfigInfo();
    configInfo.pipelineLayout = m_pipelineLayout;
    m_renderPipeline = std::make_unique<RenderPipeline>(m_logicalDevice, configInfo);
    HGINFO("Created pipeline");
}

void SimpleRenderSystem::RenderObjects(GameObject::Map& gameObjects, VkCommandBuffer commandBuffer)
{
    m_renderPipeline->Bind(commandBuffer);

    for(auto& [id, obj]: gameObjects)
    {
        obj.model->Bind(commandBuffer);
        obj.model->Draw(commandBuffer);
    }
}
} // namespace Humongous
