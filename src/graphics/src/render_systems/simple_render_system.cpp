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
    CreatePipelineLayout(globalLayout);
    CreatePipeline();
    m_modelSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
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
    std::vector<VkDescriptorType> poolTypes = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
    m_descriptorPool = std::make_unique<DescriptorPoolGrowable>(m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, poolTypes);
}

void SimpleRenderSystem::CreateModelDescriptorSetLayout()
{
    DescriptorSetLayout::Builder builder{m_logicalDevice};
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_descriptorSetLayout = builder.build();
}

void SimpleRenderSystem::AllocateDescriptorSet(u32 identifier, u32 index)
{
    HGINFO("Allocating descriptor set for %d", identifier);
    m_modelSets[index].emplace(identifier, m_descriptorPool->AllocateDescriptor(m_descriptorSetLayout->GetDescriptorSetLayout()));
}

void SimpleRenderSystem::CreatePipelineLayout(VkDescriptorSetLayout globalLayout)
{
    HGINFO("Creating pipeline layout...");
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
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

        auto objId = obj.GetId();

        ModelPushConstants push{};
        push.model = obj.transform.Mat4();
        push.normal = obj.transform.NormalMatrix();
        push.vertexAddress = obj.model->GetVertexBufferAddress();

        vkCmdPushConstants(renderData.commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelPushConstants), &push);

        if(m_modelSets[renderData.frameIndex].count(obj.GetId()) == 0) { AllocateDescriptorSet(objId, renderData.frameIndex); }

        obj.model->WriteDescriptorSet(*m_descriptorSetLayout, *m_descriptorPool, m_modelSets[renderData.frameIndex][objId]);
        vkCmdBindDescriptorSets(renderData.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, 1,
                                &m_modelSets[renderData.frameIndex][objId], 0, nullptr);

        obj.model->Bind(renderData.commandBuffer);
        obj.model->Draw(renderData.commandBuffer);
    }
}
} // namespace Humongous
