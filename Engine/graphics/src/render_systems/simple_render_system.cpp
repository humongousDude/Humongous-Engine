#include "abstractions/buffer.hpp"
#include "asset_manager.hpp"
#include "globals.hpp"
#include "logger.hpp"
#include "resource_manager.hpp"
#include <render_systems/simple_render_system.hpp>

namespace Humongous
{
SimpleRenderSystem::SimpleRenderSystem(LogicalDevice& logicalDevice, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                                       const ShaderSet& shaderSet)
    : m_logicalDevice{logicalDevice}, m_pipelineLayout{VK_NULL_HANDLE}
{
    HGINFO("Creating simple render system...");
    CreatePipelineLayout(descriptorSetLayouts);
    CreatePipeline(shaderSet);
    HGINFO("Created simple render system");
}

SimpleRenderSystem::~SimpleRenderSystem()
{
    HGINFO("Destroying simple render system...");
    vkDestroyPipelineLayout(m_logicalDevice.GetVkDevice(), m_pipelineLayout, nullptr);
    m_debugBuffer.reset();
    HGINFO("Destroyed Simple render system");
}

void SimpleRenderSystem::CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& layouts)
{
    HGINFO("Creating pipeline layout...");

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(Model::PushConstantData);

    VkPushConstantRange indexRange{};
    indexRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    indexRange.offset = sizeof(Model::PushConstantData);
    indexRange.size = sizeof(n32);

    std::vector<VkPushConstantRange> ranges = {pushConstantRange, indexRange};

    auto descriptorSetLayouts = ResourceManager::GetLayoutVector();
    descriptorSetLayouts.insert(descriptorSetLayouts.begin(), layouts.begin(), layouts.end());

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<n32>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = ranges.size();
    pipelineLayoutInfo.pPushConstantRanges = ranges.data();

    if(vkCreatePipelineLayout(m_logicalDevice.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create pipeline layout");
    }

    HGINFO("Created pipeline layout");
}

void SimpleRenderSystem::CreatePipeline(const ShaderSet& shaderSet)
{
    HGINFO("Creating pipeline...");
    RenderPipeline::PipelineConfigInfo configInfo = RenderPipeline::DefaultPipelineConfigInfo();
    configInfo.pipelineLayout = m_pipelineLayout;

    configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
    configInfo.multisampleInfo.minSampleShading = 1.0;

    configInfo.vertShaderPath = shaderSet.vertShaderPath;
    configInfo.fragShaderPath = shaderSet.fragShaderPath;

    // configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;
    // configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    // configInfo.rasterizationInfo.lineWidth = 5.0f;

    m_renderPipeline = std::make_unique<RenderPipeline>(m_logicalDevice, configInfo);

    configInfo.colorBlendInfo.attachmentCount = 0;
    configInfo.colorBlendInfo.pAttachments = nullptr;
    configInfo.colorAttachmentFormat = VK_FORMAT_UNDEFINED;
    configInfo.renderingInfo.colorAttachmentCount = 0;
    configInfo.renderingInfo.pColorAttachmentFormats = nullptr;

    ShaderSet depthSet{Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "depth.vert"),
                       Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "nothing.frag")};

    configInfo.vertShaderPath = depthSet.vertShaderPath;
    configInfo.fragShaderPath = depthSet.fragShaderPath;
    configInfo.rasterizationInfo.depthBiasEnable = VK_TRUE;
    configInfo.rasterizationInfo.depthBiasClamp = 0.0f;           // Optional
    configInfo.rasterizationInfo.depthBiasConstantFactor = 0.01f; // Optional
    configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;     // Optional

    m_depthOnlyPipeline = std::make_unique<RenderPipeline>(m_logicalDevice, configInfo);
    HGINFO("Created pipeline");

    m_debugBuffer = std::make_unique<Buffer>(&m_logicalDevice, 1, sizeof(n32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_AUTO);
    m_debugBuffer->Map();
    m_debugBuffer->WriteToBuffer(&m_verticesDrawn);
    m_debugBuffer->Flush();
    m_debugBuffer->UnMap();
}

void SimpleRenderSystem::RenderObjects(RenderData& renderData, const bool& depthOnly)
{
    if(depthOnly) { m_depthOnlyPipeline->Bind(renderData.commandBuffer); }
    else { m_renderPipeline->Bind(renderData.commandBuffer); }

    VkDescriptorSet  debugSet;
    auto             info = m_debugBuffer->DescriptorInfo();
    DescriptorWriter writer{*ResourceManager::GetModelDescriptors().debugLayout, ResourceManager::GetDescriptorPools().debugPool.get()};
    writer.WriteBuffer(0, &info).Build(debugSet);

    std::vector<VkDescriptorSet> allSets{};
    allSets.insert(allSets.begin(), renderData.uboSets.begin(), renderData.uboSets.end());
    allSets.insert(allSets.begin() + allSets.size(), renderData.sceneSets.begin(), renderData.sceneSets.end());
    allSets.insert(allSets.begin() + allSets.size(), renderData.skyboxSets.begin(), renderData.skyboxSets.end());

    vkCmdBindDescriptorSets(renderData.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                            static_cast<n32>(Globals::DescriptorSetIndices::Camera), allSets.size(), allSets.data(), 0, nullptr);

    vkCmdBindDescriptorSets(renderData.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                            static_cast<n32>(Globals::DescriptorSetIndices::Debug), 1, &debugSet, 0, nullptr);

    n32 objectsDrawn = 0;
    for(auto& [id, obj]: renderData.gameObjects)
    {
        Model::PushConstantData data{};
        data.model = obj->transform.Mat4();
        obj->model->GetVertexBuffer().UpdateAddress(obj->model->GetVertexBuffer().GetUsageFlags());
        data.vertexAddress = obj->model->GetVertexBuffer().GetDeviceAddress();
        data.id = objectsDrawn;

        vkCmdPushConstants(renderData.commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Model::PushConstantData), &data);

        obj->model->Draw(renderData.commandBuffer, m_pipelineLayout);

        objectsDrawn++;
    }

    // For some reason, this just won't work. Clearing the debug buffer at the start of the frame cause this to return 0. If we don't clear it, it
    // returns incorrect numbers
    m_verticesDrawn = objectsDrawn;

    // m_debugBuffer->Map();
    // n32 v = *static_cast<n32*>(m_debugBuffer->GetMappedMemory());
    // m_verticesDrawn = v;
    // // HGINFO("%i", v);
    // m_debugBuffer->UnMap();
}

} // namespace Humongous
