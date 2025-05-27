#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "globals.hpp"
#include "logger.hpp"
#include "resource_manager.hpp"
#include <render_systems/simple_render_system.hpp>

namespace Humongous
{
SimpleRenderSystem::SimpleRenderSystem(LogicalDevice& logicalDevice, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
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
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(m_pipelineLayout, nullptr);
    m_debugBuffer.reset();
    HGINFO("Destroyed Simple render system");
}

void SimpleRenderSystem::CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& layouts)
{
    HGINFO("Creating pipeline layout...");

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(Model::PushConstantData);

    vk::PushConstantRange indexRange{};
    indexRange.stageFlags = vk::ShaderStageFlagBits::eFragment;
    indexRange.offset = sizeof(Model::PushConstantData);
    indexRange.size = sizeof(n32);

    std::vector<vk::PushConstantRange> ranges = {pushConstantRange, indexRange};

    auto descriptorSetLayouts = ResourceManager::GetLayoutVector();
    descriptorSetLayouts.insert(descriptorSetLayouts.begin(), layouts.begin(), layouts.end());

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<n32>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = ranges.size();
    pipelineLayoutInfo.pPushConstantRanges = ranges.data();

    if(m_logicalDevice.GetVkDevice().createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_pipelineLayout) != vk::Result::eSuccess)
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

    configInfo.multisampleInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
    configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
    configInfo.multisampleInfo.minSampleShading = 1.0;

    configInfo.vertShaderPath = shaderSet.vertShaderPath;
    configInfo.fragShaderPath = shaderSet.fragShaderPath;

    m_renderPipeline = std::make_unique<RenderPipeline>(m_logicalDevice, configInfo);

    configInfo.colorBlendInfo.attachmentCount = 0;
    configInfo.colorBlendInfo.pAttachments = nullptr;
    configInfo.colorAttachmentFormat = vk::Format::eUndefined;
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

    m_debugBuffer =
        std::make_unique<Buffer>(&m_logicalDevice, 1, sizeof(n32), vk::BufferUsageFlagBits::eStorageBuffer,
                                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO);
    m_debugBuffer->Map();
    m_debugBuffer->WriteToBuffer(&m_verticesDrawn);
    m_debugBuffer->Flush();
    m_debugBuffer->UnMap();
}

void SimpleRenderSystem::RenderObjects(RenderData& renderData, const bool& depthOnly)
{
    if(depthOnly) { m_depthOnlyPipeline->Bind(renderData.commandBuffer); }
    else { m_renderPipeline->Bind(renderData.commandBuffer); }

    std::vector<vk::DescriptorSet> globalPassSets;
    if(!renderData.uboSets.empty()) { globalPassSets.insert(globalPassSets.end(), renderData.uboSets.begin(), renderData.uboSets.end()); }
    if(!renderData.sceneSets.empty()) { globalPassSets.insert(globalPassSets.end(), renderData.sceneSets.begin(), renderData.sceneSets.end()); }
    if(!renderData.skyboxSets.empty()) { globalPassSets.insert(globalPassSets.end(), renderData.skyboxSets.begin(), renderData.skyboxSets.end()); }

    if(!globalPassSets.empty())
    {
        renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout,
                                                    static_cast<n32>(Globals::DescriptorSetIndices::Camera), // Start binding index
                                                    static_cast<uint32_t>(globalPassSets.size()), globalPassSets.data(), 0, nullptr);
    }

    vk::DescriptorSet debugSet;
    auto              debugBufferInfo = m_debugBuffer->DescriptorInfo();
    DescriptorWriter  debugWriter{*ResourceManager::GetModelDescriptors().debugLayout, ResourceManager::GetDescriptorPools().debugPool.get()};
    debugWriter.WriteBuffer(0, &debugBufferInfo).Build(debugSet);

    renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout,
                                                static_cast<n32>(Globals::DescriptorSetIndices::Debug), 1, &debugSet, 0, nullptr);

    if(!depthOnly) { ResourceManager::BindGlobalDescriptorSets(renderData.commandBuffer, m_pipelineLayout); }

    n32 objectsDrawn = 0;
    if(renderData.visibleEntities)
    {
        for(const Utils::VisibleEntityInfo& entityInfo: *renderData.visibleEntities)
        {
            Humongous::EntityID entityId = entityInfo.id;

            TransformComponent* transformComp = renderData.world.GetComponent<TransformComponent>(entityId);
            ModelComponent*     modelComp = renderData.world.GetComponent<ModelComponent>(entityId);

            auto modelAsset = ResourceManager::GetModel(modelComp->modelHandle);

            Model::PushConstantData pushData{};
            pushData.model = transformComp->Mat4();

            pushData.vertexAddress = modelAsset->GetVertexBuffer().GetDeviceAddress();

            renderData.commandBuffer.pushConstants(m_pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(Model::PushConstantData),
                                                   &pushData);

            modelAsset->Draw(renderData.commandBuffer, m_pipelineLayout);

            objectsDrawn++;
        }
    }

    m_verticesDrawn = objectsDrawn; // Or more accurately, sum of vertices from drawn objects
}

} // namespace Humongous
