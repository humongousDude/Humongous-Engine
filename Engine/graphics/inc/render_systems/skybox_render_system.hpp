#pragma once

#include "logical_device.hpp"
#include "render_pipeline.hpp"
#include "skybox.hpp"

#include <memory>
#include <string>

namespace Humongous
{

class SkyboxRenderSystem
{
public:
    SkyboxRenderSystem(const ILogicalDevice& logicalDevice, class ResourceManager& resourceManager, const IAssetManager& assetManager,
                       const std::string& skyboxImgPath, const std::vector<vk::DescriptorSetLayout>& globalLayouts);
    ~SkyboxRenderSystem();

    void RenderSkybox(const u32& frameIndex, const std::vector<vk::DescriptorSet>& globalSets, vk::CommandBuffer commandBuffer);

    std::shared_ptr<Skybox> GetSkybox() const { return m_skybox; }

private:
    const ILogicalDevice&           m_logicalDevice;
    class ResourceManager&          m_resourceManager;
    const IAssetManager&            m_assetManager;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    vk::PipelineLayout              m_pipelineLayout;

    std::shared_ptr<Skybox> m_skybox;

    void CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& globalLayouts);
    void CreatePipeline();
    void InitSkybox(const std::string& skyBoxImgPath);
};
} // namespace Humongous
