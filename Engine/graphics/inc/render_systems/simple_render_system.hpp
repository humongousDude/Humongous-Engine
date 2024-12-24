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
    n32                          frameIndex;
    Camera&                      cam;
    const glm::vec3              camPos;
};

struct ShaderSet
{
    std::string vertShaderPath;
    std::string fragShaderPath;
};

class SimpleRenderSystem
{
public:
    SimpleRenderSystem(LogicalDevice& logicalDevice, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, const ShaderSet& shaderSet);
    ~SimpleRenderSystem();

    void RenderObjects(RenderData& renderData);
    s16  GetObjectsDrawn() { return m_objectsDrawn; }

private:
    LogicalDevice&                  m_logicalDevice;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    VkPipelineLayout                m_pipelineLayout{};
    s16                             m_objectsDrawn{0};

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
    void AllocateDescriptorSet(n32 identifier, n32 index);
    void CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
    void CreatePipeline(const ShaderSet& shaderSet);
};
} // namespace Humongous
