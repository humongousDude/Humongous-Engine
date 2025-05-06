#include "asset_manager.hpp"
#include "defines.hpp"
#include "extra.hpp"
#include "logger.hpp"
#include <model.hpp>
#include <render_pipeline.hpp>

using namespace Humongous::Utils;

namespace Humongous
{
RenderPipeline::RenderPipeline(LogicalDevice& logicalDevice, const RenderPipeline::PipelineConfigInfo& configinfo) : m_logicalDevice{logicalDevice}
{
    CreateRenderPipeline(configinfo);
}

RenderPipeline::~RenderPipeline()
{
    vkDestroyPipeline(m_logicalDevice.GetVkDevice(), m_pipeline, nullptr);
    HGINFO("Destroyed Render Pipeline");
}

void RenderPipeline::CreateRenderPipeline(const RenderPipeline::PipelineConfigInfo& configInfo)
{
    HGINFO("Creating Render Pipeline...");
    HGINFO("Reading shader files...");
    auto vertCode = ReadFile(configInfo.vertShaderPath);
    auto fragCode = ReadFile(configInfo.fragShaderPath);
    HGINFO("Successfully read shader files");

    vk::ShaderModule vertShaderModule;
    vk::ShaderModule fragShaderModule;

    CreateShaderModule(vertCode, &vertShaderModule);
    CreateShaderModule(fragCode, &fragShaderModule);

    HGINFO("Successfully created shader modules");

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 0;      // MUST be 0
    vertexInputInfo.pVertexBindingDescriptions = nullptr;   // MUST be nullptr
    vertexInputInfo.vertexAttributeDescriptionCount = 0;    // MUST be 0
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // MUST be nullptr
    vertexInputInfo.pNext = nullptr;

    if(!configInfo.bindless)
    {
        if(configInfo.inputBindings.size() == 0)
        {
            HGERROR("Trying to make a non-bindless render pipeline, but no vertex input bindings were specified!");
        }
        if(configInfo.attribBindings.size() == 0)
        {
            HGERROR("Trying to make a non-bindless render pipeline, but no vertex attribute bindings were specified!");
        }

        vertexInputInfo.vertexBindingDescriptionCount = static_cast<n32>(configInfo.inputBindings.size());
        vertexInputInfo.pVertexBindingDescriptions = configInfo.inputBindings.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<n32>(configInfo.attribBindings.size());
        vertexInputInfo.pVertexAttributeDescriptions = configInfo.attribBindings.data();
    }

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pNext = &configInfo.renderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;

    pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
    pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
    pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
    pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
    pipelineInfo.pDynamicState = &configInfo.dynamicStateInfo;

    pipelineInfo.layout = configInfo.pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;

    // pipelineInfo.subpass = configInfo.subpass;

    if(m_logicalDevice.GetVkDevice().createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create graphics pipeline!");
    }

    HGINFO("Successfully created graphics pipeline");

    vkDestroyShaderModule(m_logicalDevice.GetVkDevice(), vertShaderModule, nullptr);
    vkDestroyShaderModule(m_logicalDevice.GetVkDevice(), fragShaderModule, nullptr);

    HGINFO("Successfully destroyed shader modules");
}

void RenderPipeline::CreateShaderModule(const std::vector<char>& code, vk::ShaderModule* shaderModule)
{
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const n32*>(code.data());

    if(m_logicalDevice.GetVkDevice().createShaderModule(&createInfo, nullptr, shaderModule) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create shader module!");
    }
}

RenderPipeline::PipelineConfigInfo RenderPipeline::DefaultPipelineConfigInfo()
{
    using namespace Systems;
    using namespace vk;

    PipelineConfigInfo configInfo{.vertShaderPath = AssetManager::GetAsset(AssetManager::AssetType::SHADER, "simple.vert"),
                                  .fragShaderPath = AssetManager::GetAsset(AssetManager::AssetType::SHADER, "unlit.frag"),
                                  .bindless = true};

    configInfo.inputAssemblyInfo.topology = PrimitiveTopology::eTriangleList;
    configInfo.inputAssemblyInfo.primitiveRestartEnable = false;

    configInfo.rasterizationInfo.depthClampEnable = false;
    configInfo.rasterizationInfo.rasterizerDiscardEnable = false;
    configInfo.rasterizationInfo.polygonMode = PolygonMode::eFill;
    configInfo.rasterizationInfo.lineWidth = 1.0f;
    configInfo.rasterizationInfo.cullMode = CullModeFlagBits::eNone;
    configInfo.rasterizationInfo.frontFace = FrontFace::eCounterClockwise;
    configInfo.rasterizationInfo.depthBiasEnable = false;
    configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f; // Optional
    configInfo.rasterizationInfo.depthBiasClamp = 0.0f;          // Optional
    configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;    // Optional

    configInfo.multisampleInfo.sampleShadingEnable = false;
    configInfo.multisampleInfo.rasterizationSamples = SampleCountFlagBits::e1;
    configInfo.multisampleInfo.minSampleShading = 1.0f;       // Optional
    configInfo.multisampleInfo.pSampleMask = nullptr;         // Optional
    configInfo.multisampleInfo.alphaToCoverageEnable = false; // Optional
    configInfo.multisampleInfo.alphaToOneEnable = false;      // Optional

    configInfo.colorBlendAttachment.colorWriteMask =
        ColorComponentFlagBits::eR | ColorComponentFlagBits::eG | ColorComponentFlagBits::eB | ColorComponentFlagBits::eA;
    configInfo.colorBlendAttachment.blendEnable = true;
    configInfo.colorBlendAttachment.srcColorBlendFactor = BlendFactor::eSrcAlpha;
    configInfo.colorBlendAttachment.dstColorBlendFactor = BlendFactor::eOneMinusSrcAlpha;
    configInfo.colorBlendAttachment.colorBlendOp = BlendOp::eAdd;
    configInfo.colorBlendAttachment.srcAlphaBlendFactor = BlendFactor::eOneMinusSrcAlpha;
    configInfo.colorBlendAttachment.dstAlphaBlendFactor = BlendFactor::eZero;
    configInfo.colorBlendAttachment.alphaBlendOp = BlendOp::eAdd;

    configInfo.colorBlendInfo.logicOpEnable = false;
    configInfo.colorBlendInfo.logicOp = LogicOp::eCopy; // Optional
    configInfo.colorBlendInfo.attachmentCount = 1;
    configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
    configInfo.colorBlendInfo.blendConstants[0] = 0.0f; // Optional
    configInfo.colorBlendInfo.blendConstants[1] = 0.0f; // Optional
    configInfo.colorBlendInfo.blendConstants[2] = 0.0f; // Optional
    configInfo.colorBlendInfo.blendConstants[3] = 0.0f; // Optional

    configInfo.depthStencilInfo.depthTestEnable = true;
    configInfo.depthStencilInfo.depthWriteEnable = true;
    configInfo.depthStencilInfo.depthCompareOp = CompareOp::eLess;
    configInfo.depthStencilInfo.depthBoundsTestEnable = false;
    configInfo.depthStencilInfo.minDepthBounds = 0.0f; // Optional
    configInfo.depthStencilInfo.maxDepthBounds = 1.0f; // Optional
    configInfo.depthStencilInfo.stencilTestEnable = false;
    configInfo.depthStencilInfo.front = StencilOpState{}; // Optional
    configInfo.depthStencilInfo.back = StencilOpState{};  // Optional

    configInfo.dynamicStateEnables = {DynamicState::eViewport, DynamicState::eScissor};
    configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();
    configInfo.dynamicStateInfo.dynamicStateCount = static_cast<n32>(configInfo.dynamicStateEnables.size());

    configInfo.renderingInfo.viewMask = 0;
    configInfo.renderingInfo.colorAttachmentCount = 1;
    configInfo.renderingInfo.pColorAttachmentFormats = &configInfo.colorAttachmentFormat;
    configInfo.renderingInfo.depthAttachmentFormat = Format::eD32Sfloat;

    // hardcoded for now
    configInfo.colorAttachmentFormat = Format::eR16G16B16A16Sfloat;
    configInfo.renderingInfo.pColorAttachmentFormats = &configInfo.colorAttachmentFormat;
    configInfo.renderingInfo.depthAttachmentFormat = Format::eD32Sfloat;

    return configInfo;
}

} // namespace Humongous
