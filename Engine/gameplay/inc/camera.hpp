#pragma once

#include "abstractions/buffer.hpp"

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
    glm::vec3 cameraPos;
};

struct UboParams
{
    glm::vec4 lightDir = glm::vec4(1.0f, 2.0f, 0.0f, 1.0f);

    float exposure = 100.0f, gamma = 100.0f, prefilteredCubeMipLevels = 100.f, scaleIBLAmbient = 100.0f, debugViewInputs = 0, debugViewEquation = 0;
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

    VkDescriptorSet GetDescriptorSet(u32 index) const { return m_projectionMatrixSet[index]; };
    VkDescriptorSet GetParamDescriptorSet(u32 index) const { return m_uboParamSet[index]; };

    VkDescriptorSetLayout              GetParamDescriptorSetLayout() const { return m_paramLayout->GetDescriptorSetLayout(); };
    const std::vector<VkDescriptorSet> GetCombinedSets(u32 index) const { return {m_projectionMatrixSet[index], m_uboParamSet[index]}; };
    VkDescriptorSetLayout              GetDescriptorSetLayout() const { return m_projectionLayout->GetDescriptorSetLayout(); };
    VkBuffer                           GetProjectionBuffer(u32 index) const { return m_projectionBuffers[index]->GetBuffer(); };

    const glm::mat4& GetProjection() const { return m_projectionMatrix; };
    const glm::mat4& GetView() const { return m_viewMatrix; };

    void UpdateUBO(u32 index, const glm::vec3& camPos);

    static bool            IsAABBInFrustum(const glm::mat4& AABB, const std::vector<glm::vec4>& frustumPlanes);
    std::vector<glm::vec4> CalculateFrustumPlanes();

private:
    std::vector<std::unique_ptr<Buffer>> m_projectionBuffers;
    std::vector<std::unique_ptr<Buffer>> m_paramBuffers;

    std::unique_ptr<DescriptorPool>      m_projectionPool;
    std::unique_ptr<DescriptorSetLayout> m_projectionLayout;

    std::unique_ptr<DescriptorSetLayout> m_paramLayout;

    std::vector<VkDescriptorSet> m_projectionMatrixSet;
    std::vector<VkDescriptorSet> m_uboParamSet;

    glm::mat4 m_projectionMatrix{1.f};
    glm::mat4 m_viewMatrix{1.0f};

    void InitDescriptorThings(LogicalDevice* logicalDevice);
};
} // namespace Humongous
