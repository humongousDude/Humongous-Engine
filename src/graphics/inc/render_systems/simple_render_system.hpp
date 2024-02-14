#pragma once

#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool_growable.hpp"
#include <gameobject.hpp>
#include <memory>
#include <render_pipeline.hpp>

namespace Humongous
{
struct RenderData
{
    VkCommandBuffer  commandBuffer;
    VkDescriptorSet  globalSet;
    GameObject::Map& gameObjects;
    u32              frameIndex;
};

class SimpleRenderSystem
{
public:
    SimpleRenderSystem(LogicalDevice& logicalDevice, VkDescriptorSetLayout globalLayout);
    ~SimpleRenderSystem();

    void RenderObjects(RenderData& renderData);

private:
    LogicalDevice&                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    VkPipelineLayout                m_pipelineLayout;

    struct DescriptorLayouts
    {
        std::unique_ptr<DescriptorSetLayout> node;
        std::unique_ptr<DescriptorSetLayout> material;
        std::unique_ptr<DescriptorSetLayout> materialBuffers;

    } m_descriptorSetLayouts;

    std::unique_ptr<DescriptorPoolGrowable> m_imageSamplerPool;
    std::unique_ptr<DescriptorPoolGrowable> m_uniformPool;
    std::unique_ptr<DescriptorPoolGrowable> m_storagePool;

    // this is fucking ugly as sin. but i cant figure out any other way around this issue
    // 3 maps, for each swapchain image. they're all basically copies, but we write to a different one
    // to avoid writing a descriptor set thats pending
    std::vector<std::unordered_map<u32, VkDescriptorSet>> m_modelSets;

    void CreateModelDescriptorSetPool();
    void CreateModelDescriptorSetLayout();
    void AllocateDescriptorSet(u32 identifier, u32 index);
    void CreatePipelineLayout(VkDescriptorSetLayout globalLayout);
    void CreatePipeline();
};
} // namespace Humongous
