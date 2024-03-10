#include "render_systems/skybox_render_system.hpp"

#include "logger.hpp"
#include <vector>

namespace Humongous
{

SkyboxRenderSystem::SkyboxRenderSystem(LogicalDevice* logicalDevice, const std::string& skyboxImgPath,
                                       const std::vector<VkDescriptorSetLayout>& globalLayouts)
    : m_logicalDevice{logicalDevice}
{
    InitDescriptors();
    CreatePipelineLayout(globalLayouts);
    CreatePipeline();
    InitSkybox();
}

SkyboxRenderSystem::~SkyboxRenderSystem() { vkDestroyPipelineLayout(m_logicalDevice->GetVkDevice(), m_pipelineLayout, nullptr); }

void SkyboxRenderSystem::InitDescriptors()
{
    std::vector<VkDescriptorType> descs = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
    m_skyboxPool = std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 6, 0, descs);

    DescriptorSetLayout::Builder builder{*m_logicalDevice};
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_skyboxSetLayout = builder.build();
}

void SkyboxRenderSystem::CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& globalLayouts)
{
    VkPushConstantRange range{};
    range.size = sizeof(VkDeviceAddress);
    range.offset = 0;
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::vector<VkDescriptorSetLayout> layouts;
    layouts.insert(layouts.begin(), globalLayouts.begin(), globalLayouts.end());
    layouts.push_back(m_skyboxSetLayout->GetDescriptorSetLayout());

    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.flags = 0;
    layoutCI.pSetLayouts = layouts.data();
    layoutCI.setLayoutCount = layouts.size();
    layoutCI.pPushConstantRanges = &range;
    layoutCI.pushConstantRangeCount = 1;

    if(vkCreatePipelineLayout(m_logicalDevice->GetVkDevice(), &layoutCI, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create pipeline layout for skybox");
    }
}

void SkyboxRenderSystem::CreatePipeline()
{
    RenderPipeline::PipelineConfigInfo ppCI = RenderPipeline::DefaultPipelineConfigInfo();
    ppCI.depthStencilInfo.depthTestEnable = VK_FALSE;
    ppCI.depthStencilInfo.depthWriteEnable = VK_FALSE;
    ppCI.pipelineLayout = m_pipelineLayout;
    ppCI.vertShaderPath = "compiledShaders/skybox.vert.glsl.spv";
    ppCI.fragShaderPath = "compiledShaders/skybox.frag.glsl.spv";

    m_renderPipeline = std::make_unique<RenderPipeline>(*m_logicalDevice, ppCI);
}

void SkyboxRenderSystem::InitSkybox()
{
    SkyboxCreateInfo skyboxCI{.logicalDevice = m_logicalDevice,
                              .cubemapPath = "textures/papermill.ktx",
                              .descriptorSetLayout = *m_skyboxSetLayout,
                              .growablePool = *m_skyboxPool};

    m_skybox = std::make_unique<Skybox>(skyboxCI);
}

void SkyboxRenderSystem::RenderSkybox(const u32& frameIndex, std::vector<VkDescriptorSet>& globalSets, VkCommandBuffer cmd)
{
    m_renderPipeline->Bind(cmd);

    auto devAddress = m_skybox->GetVertexBufferAddress();
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &devAddress);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, globalSets.data(), 0, nullptr);

    VkDescriptorSet descSet = m_skybox->GetDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, 1, &descSet, 0, nullptr);

    m_skybox->Draw(cmd);
}

} // namespace Humongous
