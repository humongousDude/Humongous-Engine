#pragma once

#include "logical_device.hpp"
#include <non_copyable.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Humongous
{
class RenderPipeline : NonCopyable
{
public:
    struct PipelineConfigInfo
    {
        std::string vertShaderPath;
        std::string fragShaderPath;
        b32         bindless;

        b32         useMeshShaders{false};
        std::string meshShaderPath;
        std::string taskShaderPath;

        std::vector<vk::VertexInputBindingDescription>   inputBindings;
        std::vector<vk::VertexInputAttributeDescription> attribBindings;

        vk::PipelineInputAssemblyStateCreateInfo           inputAssemblyInfo;
        vk::PipelineRasterizationStateCreateInfo           rasterizationInfo;
        vk::PipelineMultisampleStateCreateInfo             multisampleInfo;
        vk::PipelineColorBlendAttachmentState              colorBlendAttachment;
        vk::PipelineColorBlendStateCreateInfo              colorBlendInfo;
        vk::PipelineDepthStencilStateCreateInfo            depthStencilInfo;
        std::vector<vk::DynamicState>                      dynamicStateEnables;
        vk::PipelineDynamicStateCreateInfo                 dynamicStateInfo;
        vk::PipelineLayout                                 pipelineLayout = nullptr;
        vk::PipelineRenderingCreateInfo                    renderingInfo;
        vk::Format                                         colorAttachmentFormat;
        std::vector<vk::Format>                            colorAttachmentFormats;
        std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments;
    };

    RenderPipeline(const LogicalDevice& logicalDevice, const PipelineConfigInfo& configInfo);
    ~RenderPipeline();

    static PipelineConfigInfo DefaultPipelineConfigInfo();

    vk::Pipeline& GetPipeline() { return m_pipeline; }

    void Bind(vk::CommandBuffer cmd) { vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline); };

private:
    const LogicalDevice& m_logicalDevice;
    vk::Pipeline         m_pipeline;

    void CreateRenderPipeline(const PipelineConfigInfo& configInfo);
    void CreateShaderModule(const std::vector<char>& code, vk::ShaderModule* shaderModule);
};
} // namespace Humongous
