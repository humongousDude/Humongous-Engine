#pragma once

#include "logical_device.hpp"
#include <non_copyable.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Humongous
{
class RenderPipeline : NonCopyable
{
public:
    struct PipelineConfigInfo
    {
        // VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo   multisampleInfo;
        VkPipelineColorBlendAttachmentState    colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo    colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo  depthStencilInfo;
        std::vector<VkDynamicState>            dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo       dynamicStateInfo;
        VkPipelineLayout                       pipelineLayout = nullptr;
        VkPipelineRenderingCreateInfo          renderingInfo;
        VkFormat                               colorAttachmentFormat;
        // VkRenderPass renderPass = nullptr;
        // uint32_t subpass = 0;
    };

    RenderPipeline(LogicalDevice& logicalDevice, const PipelineConfigInfo& configInfo);
    ~RenderPipeline();

    static PipelineConfigInfo DefaultPipelineConfigInfo();

    VkPipeline& GetPipeline() { return m_pipeline; }

    void Bind(VkCommandBuffer cmd) { vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline); };

private:
    LogicalDevice& m_logicalDevice;

    VkPipeline m_pipeline;

    void CreateRenderPipeline(const PipelineConfigInfo& configInfo);
    void CreateShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);
};
} // namespace Humongous
