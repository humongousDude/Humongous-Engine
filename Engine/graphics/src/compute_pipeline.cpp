#include "compute_pipeline.hpp"
#include "extra.hpp"

namespace Humongous
{

ComputePipeline::ComputePipeline(const ComputePipeline::ComputePipelineCreateInfo& createInfo)
    : m_logicalDevice{createInfo.logicalDevice}, m_pipeline{VK_NULL_HANDLE}
{
    CreatePipeline(createInfo);
}

ComputePipeline::~ComputePipeline()
{
    if(m_pipeline != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroyPipeline(m_pipeline, nullptr); }
}

void ComputePipeline::CreatePipeline(const ComputePipeline::ComputePipelineCreateInfo& createInfo)
{
    HGINFO("Creating compute pipeline...");
    auto shaderMod = Utils::CreateShaderModule(m_logicalDevice, createInfo.shaderFile);

    vk::PipelineShaderStageCreateInfo stageInfo{};
    stageInfo.stage = vk::ShaderStageFlagBits::eCompute;
    stageInfo.module = shaderMod;
    stageInfo.pName = "main";

    vk::ComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.layout = createInfo.pipelineLayout;
    pipelineInfo.stage = stageInfo;

    if(m_logicalDevice.CreateComputePipeline(pipelineInfo, &m_pipeline) != vk::Result::eSuccess)
    {
        HGFATAL("Failed to create renderer compute pipeline!");
    }

    m_logicalDevice.DestroyShaderModule(shaderMod);

    HGINFO("Created compute pipeline");
}

void ComputePipeline::BindPipeline(vk::CommandBuffer cmd)
{
    if(m_pipeline == VK_NULL_HANDLE)
    {
        HGERROR("Unable to bind compute pipeline, pipeline is null");
        return;
    }
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline);
}

} // namespace Humongous
