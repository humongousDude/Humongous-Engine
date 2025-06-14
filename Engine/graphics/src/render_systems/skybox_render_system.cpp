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
    CreatePipelineLayout(globalLayouts);
    CreatePipeline();
    InitSkybox(skyboxImgPath);
    HGINFO("Initialized skybox render system");
}

SkyboxRenderSystem::~SkyboxRenderSystem() { vkDestroyPipelineLayout(m_logicalDevice->GetVkDevice(), m_pipelineLayout, nullptr); }

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
    layoutCI.pSetLayouts = layouts.data();
    layoutCI.setLayoutCount = layouts.size();
    layoutCI.pPushConstantRanges = nullptr;
    layoutCI.pushConstantRangeCount = 0;

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
    ppCI.multisampleInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
    ppCI.multisampleInfo.sampleShadingEnable = VK_FALSE;
    ppCI.multisampleInfo.minSampleShading = 1.0;
    ppCI.pipelineLayout = m_pipelineLayout;

    ppCI.vertShaderPath = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "skybox.vert");
    ppCI.fragShaderPath = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "skybox.frag");

    ppCI.colorAttachmentFormat = vk::Format::eR16G16B16A16Sfloat;
    ppCI.renderingInfo.depthAttachmentFormat = vk::Format::eD32Sfloat;

    m_renderPipeline = std::make_unique<RenderPipeline>(*m_logicalDevice, ppCI);
}

void SkyboxRenderSystem::InitSkybox(const std::string& skyBoxImgPath) { m_skybox = ResourceManager::LoadSkybox("papermill"); }

void SkyboxRenderSystem::RenderSkybox(const n32& frameIndex, std::vector<vk::DescriptorSet>& globalSets, vk::CommandBuffer cmd)
{
    m_renderPipeline->Bind(cmd);

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, globalSets.data(), 0, nullptr);

    vk::DescriptorSet descSet = m_skybox->GetDescriptorSet();
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 1, 1, &descSet, 0, nullptr);

    m_skybox->Draw(cmd);
}

} // namespace Humongous
