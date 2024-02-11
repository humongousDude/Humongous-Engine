/* #include "abstractions/descriptor_layout.hpp"
#include <logger.hpp>
#include <material.hpp>
#include <model.hpp>

namespace Humongous
{
void GLTFMetallicRoughness::BuildPipelines(LogicalDevice& logicalDevice, VkDescriptorSetLayout globalLayout)
{
    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(ModelPushConstants);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorSetLayout::Builder builder{logicalDevice};
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    builder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

    materialLayout = builder.build()->GetDescriptorSetLayout();

    VkDescriptorSetLayout layouts[] = {globalLayout, materialLayout};

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = layouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &range;

    VkPipelineLayout newLayout;
    if(vkCreatePipelineLayout(logicalDevice.GetVkDevice(), &layoutInfo, nullptr, &newLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create pipeline layout");
    }

    opaquePipeline->pipelineLayout = newLayout;
    transparentPipeline->pipelineLayout = newLayout;

    RenderPipeline::PipelineConfigInfo pipelineConfig = RenderPipeline::DefaultPipelineConfigInfo();
    pipelineConfig.pipelineLayout = newLayout;

    opaquePipeline->pipeline = std::make_unique<RenderPipeline>(logicalDevice, pipelineConfig);
    transparentPipeline->pipeline = std::make_unique<RenderPipeline>(logicalDevice, pipelineConfig);
}

MaterialInstance GLTFMetallicRoughness::WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources,
                                                      DescriptorPoolGrowable& descriptorAllocator)
{
    MaterialInstance matData;
    matData.type = pass;
    if(pass == MaterialPass::Transparent) { matData.pipeline = transparentPipeline; }
    else { matData.pipeline = opaquePipeline; }

    matData.materialSet = descriptorAllocator.AllocateDescriptor(materialLayout);

    auto bufInfo = resources.dataBuffer->DescriptorInfo();
    bufInfo.offset = resources.dataBufferOffset;
    writer.WriteBuffer(0, &bufInfo);
    auto imgInfo = resources.colorTexture->GetDescriptorInfo();
    writer.WriteImage(1, &imgInfo);
    auto imgInfo2 = resources.metalRoughTexture->GetDescriptorInfo();
    writer.WriteImage(2, &imgInfo2);

    writer.Build(matData.materialSet);

    return matData;
}
} // namespace Humongous */
