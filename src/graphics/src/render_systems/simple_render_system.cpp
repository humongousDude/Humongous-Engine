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
    std::vector<VkDescriptorType> t1 = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
    std::vector<VkDescriptorType> t2 = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};
    std::vector<VkDescriptorType> t3 = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};

    m_imageSamplerPool = std::make_unique<DescriptorPoolGrowable>(m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, t1);
    m_uniformPool = std::make_unique<DescriptorPoolGrowable>(m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, t2);
    m_storagePool = std::make_unique<DescriptorPoolGrowable>(m_logicalDevice, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, t3);
}

void SimpleRenderSystem::CreateModelDescriptorSetLayout()
{
    DescriptorSetLayout::Builder nodeBuilder{m_logicalDevice};
    nodeBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    m_descriptorSetLayouts.node = nodeBuilder.build();

    DescriptorSetLayout::Builder materialBufferBuilder{m_logicalDevice};
    materialBufferBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_descriptorSetLayouts.materialBuffers = materialBufferBuilder.build();

    DescriptorSetLayout::Builder materialBuilder{m_logicalDevice};
    materialBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_descriptorSetLayouts.material = materialBuilder.build();
}

void SimpleRenderSystem::AllocateDescriptorSet(u32 identifier, u32 index) {}

void SimpleRenderSystem::CreatePipelineLayout(VkDescriptorSetLayout globalLayout)
{
    HGINFO("Creating pipeline layout...");

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(Model::PushConstantData);
    HGDEBUG("push constant range size: %d, aligneof: %d", pushConstantRange.size, alignof(Model::PushConstantData));

    VkPushConstantRange indexRange{};
    indexRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    indexRange.offset = sizeof(Model::PushConstantData);
    indexRange.size = sizeof(u32);
    HGDEBUG("push constant range size: %d, aligneof: %d", indexRange.size, alignof(u32));

    std::vector<VkPushConstantRange> ranges = {pushConstantRange, indexRange};

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {globalLayout, m_descriptorSetLayouts.material->GetDescriptorSetLayout(),
                                                               m_descriptorSetLayouts.node->GetDescriptorSetLayout(),
                                                               m_descriptorSetLayouts.materialBuffers->GetDescriptorSetLayout()};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<u32>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = ranges.size();
    pipelineLayoutInfo.pPushConstantRanges = ranges.data();

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

        Model::PushConstantData data{};
        data.model = obj.transform.Mat4();
        data.vertexAddress = obj.model->GetVertexBuffer().GetDeviceAddress();

        vkCmdPushConstants(renderData.commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Model::PushConstantData), &data);

        obj.model->Init(m_descriptorSetLayouts.material.get(), m_descriptorSetLayouts.node.get(), m_descriptorSetLayouts.materialBuffers.get(),
                        m_imageSamplerPool.get(), m_uniformPool.get(), m_storagePool.get());
        obj.model->Draw(renderData.commandBuffer, m_pipelineLayout);
    }
}
} // namespace Humongous
