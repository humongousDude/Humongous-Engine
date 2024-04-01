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
    VkCommandBuffer               commandBuffer;
    std::vector<VkDescriptorSet>  uboSets;
    std::vector<VkDescriptorSet>  sceneSets;
    GameObject::Map&              gameObjects;
    const std::vector<glm::vec4>& cameraFrustumPlanes;
    u32                           frameIndex;
};

class SimpleRenderSystem
{
public:
    SimpleRenderSystem(LogicalDevice& logicalDevice, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
    ~SimpleRenderSystem();

    void RenderObjects(RenderData& renderData);

private:
    LogicalDevice&                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    VkPipelineLayout                m_pipelineLayout{};

    struct DescriptorLayouts
    {
        std::unique_ptr<DescriptorSetLayout> node;
        std::unique_ptr<DescriptorSetLayout> material;
        std::unique_ptr<DescriptorSetLayout> materialBuffers;

    } m_descriptorSetLayouts;

    std::unique_ptr<DescriptorPoolGrowable> m_imageSamplerPool;
    std::unique_ptr<DescriptorPoolGrowable> m_uniformPool;
    std::unique_ptr<DescriptorPoolGrowable> m_storagePool;

    void CreateModelDescriptorSetPool();
    void CreateModelDescriptorSetLayout();
    void AllocateDescriptorSet(u32 identifier, u32 index);
    void CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
    void CreatePipeline();
};
} // namespace Humongous
