#pragma once

#include "abstractions/descriptor_layout.hpp"
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
    SkyboxRenderSystem(LogicalDevice* logicalDevice, const std::string& skyboxImgPath, const std::vector<vk::DescriptorSetLayout>& globalLayouts);
    ~SkyboxRenderSystem();

    void RenderSkybox(const n32& frameIndex, std::vector<vk::DescriptorSet>& globalSets, vk::CommandBuffer commandBuffer);

    std::shared_ptr<Skybox> GetSkybox() const { return m_skybox; }

private:
    LogicalDevice*                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    vk::PipelineLayout              m_pipelineLayout;

    std::shared_ptr<Skybox> m_skybox;

    void CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& globalLayouts);
    void CreatePipeline();
    void InitSkybox(const std::string& skyBoxImgPath);
};
} // namespace Humongous
