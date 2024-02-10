#include "abstractions/descriptor_writer.hpp"
#include "logger.hpp"
#include <render_systems/simple_render_system.hpp>

namespace Humongous
{
SimpleRenderSystem::SimpleRenderSystem(LogicalDevice& logicalDevice, VkDescriptorSetLayout globalLayout) : m_logicalDevice(logicalDevice)
{
    HGINFO("Creating simple render system...");
    CreateModelDescriptorSetPool();
    CreateModelDescriptorSetLayout();
    AllocateDescriptorSets();
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

void SimpleRenderSystem::CreateModelDescriptorSetPool()
{
    DescriptorPool::Builder builder{m_logicalDevice};
    builder.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT);
    builder.SetPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
    builder.SetMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_descriptorPool = builder.Build();
}

void SimpleRenderSystem::CreateModelDescriptorSetLayout()
{
    DescriptorSetLayout::Builder builder{m_logicalDevice};
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_descriptorSetLayout = builder.build();
}

void SimpleRenderSystem::AllocateDescriptorSets()
{
    m_modelSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < m_modelSets.size(); i++) { DescriptorWriter{*m_descriptorSetLayout, *m_descriptorPool}.Build(m_modelSets[i]); }
}

void SimpleRenderSystem::CreatePipelineLayout(VkDescriptorSetLayout globalLayout)
{
    HGINFO("Creating pipeline layout...");
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    HGINFO("PushConstant size = %d", sizeof(ModelPushConstants));
    pushConstantRange.size = sizeof(ModelPushConstants);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {globalLayout, m_descriptorSetLayout->GetDescriptorSetLayout()};

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
        obj.model->Bind(renderData.commandBuffer);

        ModelPushConstants push{};
        push.model = obj.transform.Mat4();
        push.normal = obj.transform.NormalMatrix();
        push.vertexAddress = obj.model->GetVertexBufferAddress();

        vkCmdPushConstants(renderData.commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelPushConstants), &push);

        obj.model->WriteDescriptorSet(*m_descriptorSetLayout, *m_descriptorPool, m_modelSets[renderData.frameIndex]);
        vkCmdBindDescriptorSets(renderData.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, 1,
                                &m_modelSets[renderData.frameIndex], 0, nullptr);

        obj.model->Draw(renderData.commandBuffer);
    }
}
} // namespace Humongous
