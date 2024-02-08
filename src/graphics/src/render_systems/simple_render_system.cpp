#include "logger.hpp"
#include <render_systems/simple_render_system.hpp>

namespace Humongous
{
SimpleRenderSystem::SimpleRenderSystem(LogicalDevice& logicalDevice, VkDescriptorSetLayout globalLayout) : m_logicalDevice(logicalDevice)
{
    HGINFO("Creating simple render system...");
    CreatePipelineLayout(globalLayout);
    CreatePipeline();
    HGINFO("Created simple render system");
}

SimpleRenderSystem::~SimpleRenderSystem()
{
    HGINFO("Destroying simple render system...");
    vkDestroyPipelineLayout(m_logicalDevice.GetVkDevice(), m_pipelineLayout, nullptr);
    HGINFO("Destroyed Simple render system");
}

void SimpleRenderSystem::CreatePipelineLayout(VkDescriptorSetLayout globalLayout)
{
    HGINFO("Creating pipeline layout...");
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ModelPushConstants);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {globalLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<u32>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

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

void SimpleRenderSystem::RenderObjects(RenderData& renderData)
{
    m_renderPipeline->Bind(renderData.commandBuffer);
    vkCmdBindDescriptorSets(renderData.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &renderData.globalSet, 0, nullptr);

    for(auto& [id, obj]: renderData.gameObjects)
    {
        if(!obj.model) { continue; }

        ModelPushConstants push{};
        push.model = obj.transform.Mat4();
        push.normal = obj.transform.NormalMatrix();

        vkCmdPushConstants(renderData.commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(ModelPushConstants), &push);

        obj.model->Bind(renderData.commandBuffer);
        obj.model->Draw(renderData.commandBuffer);
    }
}
} // namespace Humongous
