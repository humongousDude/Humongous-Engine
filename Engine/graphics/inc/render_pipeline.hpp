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
        std::string vertShaderPath{};
        b32         bindless{};

        b32         useMeshShaders{false};
        std::string meshShaderPath{};
        std::string taskShaderPath{};

        b32         useFragmentShader{true};
        std::string fragShaderPath{};

        std::vector<vk::VertexInputBindingDescription>   inputBindings{};
        std::vector<vk::VertexInputAttributeDescription> attribBindings{};

        vk::PipelineInputAssemblyStateCreateInfo           inputAssemblyInfo{};
        vk::PipelineRasterizationStateCreateInfo           rasterizationInfo{};
        vk::PipelineMultisampleStateCreateInfo             multisampleInfo{};
        vk::PipelineColorBlendAttachmentState              colorBlendAttachment{};
        vk::PipelineColorBlendStateCreateInfo              colorBlendInfo{};
        vk::PipelineDepthStencilStateCreateInfo            depthStencilInfo{};
        std::vector<vk::DynamicState>                      dynamicStateEnables{};
        vk::PipelineDynamicStateCreateInfo                 dynamicStateInfo{};
        vk::PipelineRenderingCreateInfo                    renderingInfo{};
        vk::Format                                         colorAttachmentFormat{};
        std::vector<vk::Format>                            colorAttachmentFormats{};
        std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments{};

        std::vector<vk::PushConstantRange>   pushConstantRanges{};
        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{};
        vk::PipelineLayout                   pipelineLayout = VK_NULL_HANDLE;
    };

    RenderPipeline(const ILogicalDevice& logicalDevice, const PipelineConfigInfo& configInfo);
    ~RenderPipeline();

    static PipelineConfigInfo DefaultPipelineConfigInfo();

    vk::Pipeline& GetPipeline() { return m_pipeline; }

    vk::PipelineLayout& GetPipelineLayout() { return m_pipelineLayout; }

    void Bind(vk::CommandBuffer cmd) { m_logicalDevice.RecordBindPipeline(cmd, vk::PipelineBindPoint::eGraphics, m_pipeline); };

private:
    const ILogicalDevice& m_logicalDevice;
    vk::Pipeline          m_pipeline;
    vk::PipelineLayout    m_pipelineLayout;
    b8                    m_ownsLayout{false};

    void CreateLayout(const PipelineConfigInfo& configInfo);
    void CreatePipeline(const PipelineConfigInfo& configInfo);
};
} // namespace Humongous
