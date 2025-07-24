#include "render_pipeline.hpp"
#include "asset_manager.hpp"
#include "defines.hpp"
#include "extra.hpp"
#include "logger.hpp"

using namespace Humongous::Utils;

namespace Humongous
{
RenderPipeline::RenderPipeline(LogicalDevice& logicalDevice, const RenderPipeline::PipelineConfigInfo& configinfo) : m_logicalDevice{logicalDevice}
{
    CreateRenderPipeline(configinfo);
}

RenderPipeline::~RenderPipeline()
{
    HGINFO("Destroying Render pipeline...");
    vkDestroyPipeline(m_logicalDevice.GetVkDevice(), m_pipeline, nullptr);
    HGINFO("Destroyed Render Pipeline");
}

void RenderPipeline::CreateRenderPipeline(const RenderPipeline::PipelineConfigInfo& configInfo)
{
    HGINFO("Creating Render Pipeline...");
    HGINFO("Reading shader files...");

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    vk::ShaderModule vertShaderModule = VK_NULL_HANDLE;
    vk::ShaderModule fragShaderModule = VK_NULL_HANDLE;
    vk::ShaderModule meshShaderModule = VK_NULL_HANDLE;
    vk::ShaderModule taskShaderModule = VK_NULL_HANDLE;

    b8 useMeshShaders = configInfo.useMeshShaders;

    if(!m_logicalDevice.GetPhysicalDevice().GetCurrentCapabilities().supportsMeshShaders)
    {
        HGWARN("Tried to create a render pipeline with mesh shaders, but the device does not support them!");
        useMeshShaders = false;
    }

    if(useMeshShaders)
    {
        auto taskCode = ReadFile(configInfo.taskShaderPath);
        CreateShaderModule(taskCode, &taskShaderModule);
        vk::PipelineShaderStageCreateInfo taskShaderStageInfo{};
        taskShaderStageInfo.stage = vk::ShaderStageFlagBits::eTaskEXT;
        taskShaderStageInfo.module = taskShaderModule;
        taskShaderStageInfo.pName = "main";
        shaderStages.push_back(taskShaderStageInfo);

        auto meshCode = ReadFile(configInfo.meshShaderPath);
        CreateShaderModule(meshCode, &meshShaderModule);
        vk::PipelineShaderStageCreateInfo meshShaderStageInfo{};
        meshShaderStageInfo.stage = vk::ShaderStageFlagBits::eMeshEXT;
        meshShaderStageInfo.module = meshShaderModule;
        meshShaderStageInfo.pName = "main";
        shaderStages.push_back(meshShaderStageInfo);
    }
    else
    {
        auto vertCode = ReadFile(configInfo.vertShaderPath);
        CreateShaderModule(vertCode, &vertShaderModule);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
        shaderStages.push_back(vertShaderStageInfo);
    }

    auto fragCode = ReadFile(configInfo.fragShaderPath);
    CreateShaderModule(fragCode, &fragShaderModule);
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    shaderStages.push_back(fragShaderStageInfo);

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    if(!useMeshShaders)
    {
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<n32>(configInfo.inputBindings.size());
        vertexInputInfo.pVertexBindingDescriptions = configInfo.inputBindings.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<n32>(configInfo.attribBindings.size());
        vertexInputInfo.pVertexAttributeDescriptions = configInfo.attribBindings.data();
        vertexInputInfo.pNext = nullptr;
    }
    else
    {
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        vertexInputInfo.pNext = nullptr;
    }

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::Result                     result;
    vk::GraphicsPipelineCreateInfo pipelineInfoGraphics{};
    pipelineInfoGraphics.pNext = &configInfo.renderingInfo;
    pipelineInfoGraphics.stageCount = static_cast<n32>(shaderStages.size());
    pipelineInfoGraphics.pStages = shaderStages.data();
    pipelineInfoGraphics.pViewportState = &viewportState;
    pipelineInfoGraphics.pVertexInputState = nullptr;
    pipelineInfoGraphics.pInputAssemblyState = nullptr;
    pipelineInfoGraphics.pRasterizationState = &configInfo.rasterizationInfo;
    pipelineInfoGraphics.pMultisampleState = &configInfo.multisampleInfo;
    pipelineInfoGraphics.pColorBlendState = &configInfo.colorBlendInfo;
    pipelineInfoGraphics.pDepthStencilState = &configInfo.depthStencilInfo;
    pipelineInfoGraphics.pDynamicState = &configInfo.dynamicStateInfo;
    pipelineInfoGraphics.layout = configInfo.pipelineLayout;
    pipelineInfoGraphics.renderPass = VK_NULL_HANDLE;
    pipelineInfoGraphics.subpass = 0;
    pipelineInfoGraphics.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfoGraphics.basePipelineIndex = -1;

    if(!useMeshShaders)
    {
        pipelineInfoGraphics.pVertexInputState = &vertexInputInfo;
        pipelineInfoGraphics.pInputAssemblyState = &configInfo.inputAssemblyInfo;
    }

    HGINFO("Created graphics pipeline with %i render attachments", configInfo.colorBlendInfo.attachmentCount);

    result = m_logicalDevice.GetVkDevice().createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfoGraphics, nullptr, &m_pipeline);

    if(result != vk::Result::eSuccess) { HGERROR("Failed to create pipeline! Error: %s", vk::to_string(result).c_str()); }

    HGINFO("Successfully created pipeline");

    if(vertShaderModule != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroyShaderModule(vertShaderModule, nullptr); }
    if(fragShaderModule != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroyShaderModule(fragShaderModule, nullptr); }
    if(meshShaderModule != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroyShaderModule(meshShaderModule, nullptr); }
    if(taskShaderModule != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroyShaderModule(taskShaderModule, nullptr); }

    HGINFO("Successfully destroyed shader modules");
    HGINFO("Created render pipeline");
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

// NOTE: You should still assign the vertex shader path in case the used physical device does not support mesh shaders
RenderPipeline::PipelineConfigInfo RenderPipeline::DefaultPipelineConfigInfo()
{
    using namespace Systems;
    using namespace vk;

    PipelineConfigInfo configInfo{.vertShaderPath = AssetManager::GetAsset(AssetManager::AssetType::SHADER, "simple.vert"),
                                  .fragShaderPath = AssetManager::GetAsset(AssetManager::AssetType::SHADER, "pbr.frag"),
                                  .bindless = true};

    configInfo.inputAssemblyInfo.topology = PrimitiveTopology::eTriangleList;
    configInfo.inputAssemblyInfo.primitiveRestartEnable = false;

    configInfo.rasterizationInfo.depthClampEnable = false;
    configInfo.rasterizationInfo.rasterizerDiscardEnable = false;
    configInfo.rasterizationInfo.polygonMode = PolygonMode::eFill;
    configInfo.rasterizationInfo.lineWidth = 1.0f;
    configInfo.rasterizationInfo.cullMode = CullModeFlagBits::eBack;
    configInfo.rasterizationInfo.frontFace = FrontFace::eClockwise;
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

    configInfo.colorBlendAttachments.push_back(configInfo.colorBlendAttachment);
    configInfo.colorAttachmentFormats.push_back(vk::Format::eR16G16B16A16Sfloat);

    configInfo.colorBlendInfo.logicOpEnable = false;
    configInfo.colorBlendInfo.logicOp = LogicOp::eCopy; // Optional
    configInfo.colorBlendInfo.attachmentCount = configInfo.colorBlendAttachments.size();
    configInfo.colorBlendInfo.pAttachments = configInfo.colorBlendAttachments.data();
    configInfo.colorBlendInfo.blendConstants[0] = 0.0f; // Optional
    configInfo.colorBlendInfo.blendConstants[1] = 0.0f; // Optional
    configInfo.colorBlendInfo.blendConstants[2] = 0.0f; // Optional
    configInfo.colorBlendInfo.blendConstants[3] = 0.0f; // Optional

    configInfo.depthStencilInfo.depthTestEnable = true;
    configInfo.depthStencilInfo.depthWriteEnable = true;
    configInfo.depthStencilInfo.depthCompareOp = CompareOp::eGreaterOrEqual;
    configInfo.depthStencilInfo.depthBoundsTestEnable = false;
    configInfo.depthStencilInfo.minDepthBounds = 0.0f; // Optional
    configInfo.depthStencilInfo.maxDepthBounds = 1.0f; // Optional
    configInfo.depthStencilInfo.stencilTestEnable = false;
    configInfo.depthStencilInfo.front = StencilOpState{}; // Optional
    configInfo.depthStencilInfo.back = StencilOpState{};  // Optional

    configInfo.dynamicStateEnables = {DynamicState::eViewport, DynamicState::eScissor};
    configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();
    configInfo.dynamicStateInfo.dynamicStateCount = static_cast<n32>(configInfo.dynamicStateEnables.size());

    configInfo.colorAttachmentFormat = Format::eR16G16B16A16Sfloat; // Or a more suitable default like swapchain format

    // Initialize renderingInfo for a single color attachment (common default)
    configInfo.renderingInfo.viewMask = 0;
    configInfo.renderingInfo.colorAttachmentCount = configInfo.colorBlendAttachments.size();
    // Make pColorAttachmentFormats point to the *member* that owns the data
    configInfo.renderingInfo.pColorAttachmentFormats = configInfo.colorAttachmentFormats.data();

    // Always specify depth and stencil formats if relevant, even if undefined
    configInfo.renderingInfo.depthAttachmentFormat = Format::eD32SfloatS8Uint;   // Common depth/stencil format
    configInfo.renderingInfo.stencilAttachmentFormat = Format::eD32SfloatS8Uint; // Stencil must match depth if combined
    return configInfo;
}

} // namespace Humongous
