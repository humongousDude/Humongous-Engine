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
    SkyboxRenderSystem(LogicalDevice* logicalDevice, const std::string& skyboxImgPath, const std::vector<VkDescriptorSetLayout>& globalLayouts);
    ~SkyboxRenderSystem();

    void RenderSkybox(const u32& frameIndex, std::vector<VkDescriptorSet>& globalSets, VkCommandBuffer commandBuffer);

private:
    LogicalDevice*                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    VkPipelineLayout                m_pipelineLayout;

    std::unique_ptr<DescriptorPoolGrowable> m_skyboxPool;
    std::unique_ptr<DescriptorSetLayout>    m_skyboxSetLayout;

    std::unique_ptr<Skybox> m_skybox;

    void InitDescriptors();
    void CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& globalLayouts);
    void CreatePipeline();
    void InitSkybox();
};
} // namespace Humongous
