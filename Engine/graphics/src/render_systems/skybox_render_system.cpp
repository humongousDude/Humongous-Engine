#include "render_systems/skybox_render_system.hpp"

#include "asset_manager.hpp"
#include "logger.hpp"
#include "resource_manager.hpp"
#include <vector>

namespace Humongous
{

SkyboxRenderSystem::SkyboxRenderSystem(LogicalDevice* logicalDevice, const std::string& skyboxImgPath,
                                       const std::vector<vk::DescriptorSetLayout>& globalLayouts)
    : m_logicalDevice{logicalDevice}
{
    HGINFO("Initializing skybox render system...");
    InitDescriptors();
    CreatePipelineLayout(globalLayouts);
    CreatePipeline();
    InitSkybox(skyboxImgPath);
    HGINFO("Initialized skybox render system");
}

SkyboxRenderSystem::~SkyboxRenderSystem() { vkDestroyPipelineLayout(m_logicalDevice->GetVkDevice(), m_pipelineLayout, nullptr); }

void SkyboxRenderSystem::InitDescriptors()
{
    std::vector<vk::DescriptorType> descs = {vk::DescriptorType::eCombinedImageSampler};
    m_skyboxPool = std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 6, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, descs);

    DescriptorSetLayout::Builder builder{*m_logicalDevice};
    builder.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    m_skyboxSetLayout = builder.Build();
}

void SkyboxRenderSystem::CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& globalLayouts)
{
    vk::PushConstantRange range{};
    range.size = sizeof(vk::DeviceAddress);
    range.offset = 0;
    range.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::vector<vk::DescriptorSetLayout> layouts;
    layouts.insert(layouts.begin(), globalLayouts.begin(), globalLayouts.end());
    layouts.push_back(ResourceManager::GetSkyboxDescriptorLayout());

    vk::PipelineLayoutCreateInfo layoutCI{};
    // layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    // layoutCI.flags = 0;
    layoutCI.pSetLayouts = layouts.data();
    layoutCI.setLayoutCount = layouts.size();
    layoutCI.pPushConstantRanges = &range;
    layoutCI.pushConstantRangeCount = 1;

    if(m_logicalDevice->GetVkDevice().createPipelineLayout(&layoutCI, nullptr, &m_pipelineLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create pipeline layout for skybox");
    }
}

void SkyboxRenderSystem::CreatePipeline()
{
    RenderPipeline::PipelineConfigInfo ppCI = RenderPipeline::DefaultPipelineConfigInfo();
    ppCI.depthStencilInfo.depthTestEnable = VK_TRUE;
    ppCI.depthStencilInfo.depthWriteEnable = VK_FALSE;
    ppCI.depthStencilInfo.depthCompareOp = vk::CompareOp::eLessOrEqual;
    ppCI.multisampleInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
    ppCI.multisampleInfo.sampleShadingEnable = VK_FALSE;
    ppCI.multisampleInfo.minSampleShading = 1.0;
    ppCI.pipelineLayout = m_pipelineLayout;

    ppCI.vertShaderPath = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "skybox.vert");
    ppCI.fragShaderPath = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "skybox.frag");

    m_renderPipeline = std::make_unique<RenderPipeline>(*m_logicalDevice, ppCI);
}

void SkyboxRenderSystem::InitSkybox(const std::string& skyBoxImgPath) { m_skybox = ResourceManager::LoadSkybox("papermill"); }

void SkyboxRenderSystem::RenderSkybox(const n32& frameIndex, std::vector<vk::DescriptorSet>& globalSets, vk::CommandBuffer cmd)
{
    m_renderPipeline->Bind(cmd);

    auto devAddress = m_skybox->GetVertexBufferAddress();
    cmd.pushConstants(m_pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(vk::DeviceAddress), &devAddress);

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, globalSets.data(), 0, nullptr);

    vk::DescriptorSet descSet = m_skybox->GetDescriptorSet();
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 1, 1, &descSet, 0, nullptr);

    m_skybox->Draw(cmd);
}

} // namespace Humongous
