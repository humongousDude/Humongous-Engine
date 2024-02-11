#pragma once

// will remain unused for a while

/* #include "abstractions/buffer.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "images.hpp"
#include "texture.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "defines.hpp"
#include <render_pipeline.hpp>

namespace Humongous
{

enum class MaterialPass : u8
{
    MainColor,
    Transparent,
    Other
};

struct MaterialPipeline
{
    std::unique_ptr<RenderPipeline> pipeline;
    VkPipelineLayout                pipelineLayout;
};

struct MaterialInstance
{
    std::shared_ptr<RenderPipeline> pipeline;
    VkDescriptorSet                 materialSet;
    MaterialPass                    type;
};

struct GLTFMetallicRoughness
{
    std::unique_ptr<MaterialPipeline> opaquePipeline;
    std::unique_ptr<MaterialPipeline> transparentPipeline;

    VkDescriptorSetLayout materialLayout;

    struct MaterialConstants
    {
        glm::vec4 colorFactors;
        glm::vec4 metalRoughFactors;
    };

    struct MaterialResources
    {
        Texture* colorTexture;
        Texture* metalRoughTexture;
        Buffer*  dataBuffer;
        u32      dataBufferOffset;
    };

    DescriptorWriter writer;

    void BuildPipelines(LogicalDevice& logicalDevice, VkDescriptorSetLayout globalLayout);
    void ClearResources(LogicalDevice& logicalDevice);

    MaterialInstance WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources,
                                   DescriptorPoolGrowable& descriptorAllocator);
};
} // namespace Humongous */
