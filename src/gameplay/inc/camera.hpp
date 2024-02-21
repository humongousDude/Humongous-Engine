#pragma once

#include "abstractions/buffer.hpp"
#include "texture.hpp"

#include <abstractions/descriptor_layout.hpp>
#include <abstractions/descriptor_pool.hpp>
#include <abstractions/descriptor_writer.hpp>

#include <memory>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace Humongous
{
struct ProjectionUBO
{
    glm::mat4 projection;
    glm::mat4 view;
};

class Camera
{
public:
    Camera(LogicalDevice* logicalDevice);
    ~Camera();

    void SetOrthographicProjection(float left, float right, float top, float bottom, float near, float far);

    void SetPerspectiveProjection(float fovy, float spect, float near, float far);

    void SetViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.0f, -1.0f, 0.0f});
    void SetViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3{0.0f, -1.0f, 0.0f});
    void SetViewYXZ(glm::vec3 position, glm::vec3 rotation);

    VkDescriptorSet              GetDescriptorSet(u32 index) const { return m_projectionMatrixSet[index]; };
    VkDescriptorSet              GetCubemapSet() const { return m_cubeMapSet; };
    std::vector<VkDescriptorSet> GetCombinedSets(u32 index) const { return {m_projectionMatrixSet[index], m_cubeMapSet}; };
    VkDescriptorSetLayout        GetDescriptorSetLayout() const { return m_projectionLayout->GetDescriptorSetLayout(); };
    VkDescriptorSetLayout        GetCubemapLayout() const { return m_cubeMapLayout->GetDescriptorSetLayout(); }
    VkBuffer                     GetProjectionBuffer(u32 index) const { return m_projectionBuffers[index]->GetBuffer(); };

    const glm::mat4& GetProjection() const { return m_projectionMatrix; };
    const glm::mat4& GetView() const { return m_viewMatrix; };

    void UpdateUBO(u32 index);

private:
    std::vector<std::unique_ptr<Buffer>>    m_projectionBuffers;
    std::unique_ptr<DescriptorPool>         m_projectionPool;
    std::unique_ptr<DescriptorPoolGrowable> m_cubeMapPool;
    std::unique_ptr<DescriptorSetLayout>    m_projectionLayout;
    std::unique_ptr<DescriptorSetLayout>    m_cubeMapLayout;
    std::vector<VkDescriptorSet>            m_projectionMatrixSet;
    VkDescriptorSet                         m_cubeMapSet;
    std::unique_ptr<Texture>                m_cubeMap;

    glm::mat4 m_projectionMatrix{1.f};
    glm::mat4 m_viewMatrix{1.0f};

    void InitDescriptorThings(LogicalDevice* logicalDevice);
};
} // namespace Humongous
