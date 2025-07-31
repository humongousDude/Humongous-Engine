#pragma once

#include "logical_device.hpp"
#include "non_copyable.hpp"

namespace Humongous
{

class ComputePipeline : NonCopyable
{
public:
    struct ComputePipelineCreateInfo
    {
        const ILogicalDevice& logicalDevice;
        std::string           shaderFile = "";
        vk::PipelineLayout    pipelineLayout;
    };

    ComputePipeline(const ComputePipelineCreateInfo& createInfo);
    ~ComputePipeline();

    vk::Pipeline GetPipeline() const { return m_pipeline; }

    void BindPipeline(vk::CommandBuffer cmd);

private:
    const ILogicalDevice& m_logicalDevice;
    vk::Pipeline          m_pipeline = VK_NULL_HANDLE;

    void CreatePipeline(const ComputePipelineCreateInfo& createInfo);
};

} // namespace Humongous
