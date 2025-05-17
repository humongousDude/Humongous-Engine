#pragma once

#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool.hpp"

#include <array>
#include <memory>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp> // For glm::length2

namespace Humongous
{

struct alignas(16) ProjectionUBO
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 projectionView;
    glm::vec3 cameraPos;
};

struct alignas(16) UboParams
{
    glm::vec3 camPos{};
    f32       _padding0;
    glm::vec4 lightDir = glm::vec4(glm::normalize(glm::vec3(1.0f, -3.0f, 1.0f)), 0.0f);
    f32       exposure = 2.0f, gamma = 2.2f, prefilteredCubeMipLevels = 9.f, scaleIBLAmbient = 0.05f, debugViewInputs = 0, debugViewEquation = 0;
};

// Define a plane struct representing a plane in 3D space
struct Plane
{
    glm::vec3 normal;
    float     distance;
};

class Camera
{
public:
    struct CombinedCameraData
    {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 projectionView;
        glm::vec3 camPos;
    };

    Camera(LogicalDevice* logicalDevice);
    ~Camera();

    void SetOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
    void SetPerspectiveProjection(float fovy, float spect, float near, float far);

    VkDescriptorSet GetDescriptorSet(n32 index) const { return m_projectionMatrixSet[index]; };
    VkDescriptorSet GetParamDescriptorSet(n32 index) const { return m_uboParamSet[index]; };

    void DrawUI();

    VkDescriptorSetLayout              GetParamDescriptorSetLayout() const { return m_paramDescriptorLayout->GetDescriptorSetLayout(); };
    const std::vector<VkDescriptorSet> GetCombinedSets(n32 index) const { return {m_projectionMatrixSet[index], m_uboParamSet[index]}; };
    VkDescriptorSetLayout              GetDescriptorSetLayout() const { return m_projectionDescriptorLayout->GetDescriptorSetLayout(); };
    VkBuffer                           GetProjectionBuffer(n32 index) const { return m_projectionBuffers[index]->GetBuffer(); };
    Buffer&                            GetProjectionBufferHandle(n32 index) const { return *m_projectionBuffers[index]; }
    Buffer&                            GetCombinedDataBufferHandle(n32 index) const { return *m_combinedCameraDataBuffers[index]; }

    const glm::mat4& GetProjection() const { return m_projectionMatrix; };
    const glm::mat4& GetView() const { return m_viewMatrix; };

    glm::mat4 GetProjectionViewMatrix() const { return m_projectionMatrix * m_viewMatrix; }

    static void ExtractFrustumPlanes(const glm::mat4& viewProjectionMatrix, std::array<Plane, 6>& planes);
    bool        IsAABBOutsidePlane(const Plane& plane, const glm::vec3& aabbMin, const glm::vec3& aabbMax) const;
    bool        IsAABBInsideFrustum(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const;

    void UpdateViewMatrix(); // Function to calculate and update m_viewMatrix

    void Update();

    void SetPosition(glm::vec3 position) { m_position = position; }
    void SetRotation(glm::vec3 rotation) { m_rotation = rotation; }

    glm::vec3 GetPosition() const { return m_position; }
    glm::vec3 GetRotation() const { return m_rotation; }

    glm::vec3 GetForward() const { return m_forward; }
    glm::vec3 GetUp() const { return m_up; }

private:
    n32 m_index{0};

    std::vector<std::unique_ptr<Buffer>> m_projectionBuffers;
    std::vector<std::unique_ptr<Buffer>> m_paramBuffers;
    std::vector<std::unique_ptr<Buffer>> m_combinedCameraDataBuffers;

    std::unique_ptr<DescriptorPool>      m_projectionPool;
    std::unique_ptr<DescriptorSetLayout> m_projectionDescriptorLayout;
    std::unique_ptr<DescriptorSetLayout> m_paramDescriptorLayout;

    std::vector<vk::DescriptorSet> m_projectionMatrixSet;
    std::vector<vk::DescriptorSet> m_uboParamSet;
    UboParams                      m_uboParams{};

    glm::mat4 m_projectionMatrix{1.f};
    glm::mat4 m_viewMatrix{1.0f};
    glm::vec3 m_position;
    glm::vec3 m_rotation; // Store rotation as Euler angles (YXZ order - Yaw, Pitch, Roll) in radians
    glm::vec3 m_forward;
    glm::vec3 m_up;

    void InitDescriptorThings(LogicalDevice* logicalDevice);

    void UpdateUBO(n32 index);
    void UpdateCombinedCameraData(n32 index);
};
} // namespace Humongous
