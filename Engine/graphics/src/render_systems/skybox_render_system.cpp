#include "render_systems/skybox_render_system.hpp"
#include "asset_manager.hpp"
#include "logger.hpp"
#include "resource_manager.hpp"
#include <vector>

namespace Humongous
{

SkyboxRenderSystem::SkyboxRenderSystem(const ILogicalDevice& logicalDevice, class ResourceManager& resourceManager,
                                       const IAssetManager& assetManager, const std::vector<vk::DescriptorSetLayout>& globalLayouts)
    : m_logicalDevice{logicalDevice}, m_resourceManager{resourceManager}, m_assetManager{assetManager}
{
    HGINFO("Initializing skybox render system...");
    CreatePipelineLayout(globalLayouts);
    CreatePipeline();
    InitSkybox();
    HGINFO("Initialized skybox render system");
}

SkyboxRenderSystem::~SkyboxRenderSystem() { vkDestroyPipelineLayout(m_logicalDevice.GetVkDevice(), m_pipelineLayout, nullptr); }

void SkyboxRenderSystem::CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& globalLayouts)
{
    vk::PushConstantRange range{};
    range.size = sizeof(vk::DeviceAddress);
    range.offset = 0;
    range.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::vector<vk::DescriptorSetLayout> layouts;
    layouts.insert(layouts.begin(), globalLayouts.begin(), globalLayouts.end());
    layouts.push_back(m_resourceManager.GetSkyboxDescriptorLayout());

    vk::PipelineLayoutCreateInfo layoutCI{};
    layoutCI.pSetLayouts = layouts.data();
    layoutCI.setLayoutCount = layouts.size();
    layoutCI.pPushConstantRanges = nullptr;
    layoutCI.pushConstantRangeCount = 0;

    if(m_logicalDevice.GetVkDevice().createPipelineLayout(&layoutCI, nullptr, &m_pipelineLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create pipeline layout for skybox");
    }
}

void SkyboxRenderSystem::CreatePipeline()
{
    HGINFO("CREATING SKYBOX PIPELINE...");
    RenderPipeline::PipelineConfigInfo ppCI = RenderPipeline::DefaultPipelineConfigInfo();
    ppCI.pipelineLayout = m_pipelineLayout;

    ppCI.vertShaderPath = m_assetManager.GetAsset(AssetManager::AssetType::SHADER, "skybox.vert");
    ppCI.fragShaderPath = m_assetManager.GetAsset(AssetManager::AssetType::SHADER, "skybox.frag");
    ppCI.depthStencilInfo.depthTestEnable = false;
    ppCI.depthStencilInfo.depthWriteEnable = false;
    ppCI.depthStencilInfo.depthCompareOp = vk::CompareOp::eGreaterOrEqual;
    ppCI.depthStencilInfo.stencilTestEnable = true;

    ppCI.renderingInfo.stencilAttachmentFormat = vk::Format::eD32SfloatS8Uint;
    ppCI.depthStencilInfo.front.compareOp = vk::CompareOp::eEqual;
    ppCI.depthStencilInfo.front.reference = 0;
    ppCI.depthStencilInfo.front.compareMask = 0xFF;
    ppCI.depthStencilInfo.front.writeMask = 0x00;
    ppCI.depthStencilInfo.front.failOp = vk::StencilOp::eKeep;
    ppCI.depthStencilInfo.front.passOp = vk::StencilOp::eKeep;
    ppCI.depthStencilInfo.front.depthFailOp = vk::StencilOp::eKeep;
    ppCI.depthStencilInfo.back = ppCI.depthStencilInfo.front;
    m_renderPipeline = std::make_unique<RenderPipeline>(m_logicalDevice, ppCI);
    HGINFO("CREATED SKYBOX PIPELINE");
}

void SkyboxRenderSystem::InitSkybox() { m_skybox = m_resourceManager.LoadSkybox("papermill"); }

void SkyboxRenderSystem::RenderSkybox(const std::vector<vk::DescriptorSet>& globalSets, vk::CommandBuffer cmd)
{
    m_renderPipeline->Bind(cmd);

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, globalSets.data(), 0, nullptr);

    vk::DescriptorSet descSet = m_skybox->GetDescriptorSet();
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 1, 1, &descSet, 0, nullptr);

    m_skybox->Draw(cmd);
}

} // namespace Humongous
