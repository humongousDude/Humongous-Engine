#pragma once

#include "abstractions/descriptor_layout.hpp"
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

    std::unique_ptr<DescriptorSetLayout> m_descriptorSetLayout;
    std::unique_ptr<DescriptorPool>      m_descriptorPool;
    std::vector<VkDescriptorSet>         m_modelSets;

    void CreateModelDescriptorSetPool();
    void CreateModelDescriptorSetLayout();
    void AllocateDescriptorSets();
    void CreatePipelineLayout(VkDescriptorSetLayout globalLayout);
    void CreatePipeline();
};
} // namespace Humongous
