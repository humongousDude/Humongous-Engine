#pragma once

#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool_growable.hpp"
#include "camera.hpp"
#include <gameobject.hpp>
#include <memory>
#include <render_pipeline.hpp>

namespace Humongous
{
struct RenderData
{
    VkCommandBuffer              commandBuffer;
    std::vector<VkDescriptorSet> uboSets;
    std::vector<VkDescriptorSet> sceneSets;
    GameObject::Map&             gameObjects;
    u32                          frameIndex;
    Camera&                      cam;
    const glm::vec3              camPos;
};

class SimpleRenderSystem
{
public:
    SimpleRenderSystem(LogicalDevice& logicalDevice, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
    ~SimpleRenderSystem();

    void RenderObjects(RenderData& renderData);
    i16  GetObjectsDrawn() { return m_objectsDrawn; }

private:
    LogicalDevice&                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    VkPipelineLayout                m_pipelineLayout{};
    i16                             m_objectsDrawn{0};

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
