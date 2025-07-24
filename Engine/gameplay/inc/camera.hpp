#pragma once

#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool.hpp"

#include <Eigen/Dense>

#include <array>
#include <memory>

namespace Humongous
{

struct alignas(16) ProjectionUBO
{
    Eigen::Matrix4f projection;
    Eigen::Matrix4f invProjection;
    Eigen::Matrix4f view;
    Eigen::Matrix4f invView;
    Eigen::Matrix4f projectionView;
    Eigen::Vector3f cameraPos;
};

struct alignas(16) UboParams
{
    Eigen::Vector3f camPos{};
    f32             _padding0;
    Eigen::Vector4f lightDir = Eigen::Vector4f::Random();
    f32             exposure = 1.0f, gamma = 1.0f, radiance = 0.5f, prefilteredCubeMipLevels = 9.f, scaleIBLAmbient = 0.05f;
    s32             debugViewInputs = 0, debugViewEquation = 0;
};

struct Plane
{
    Eigen::Vector3f normal;
    float           distance;
};

class Camera
{
public:
    struct CombinedCameraData
    {
        Eigen::Matrix4f projection;
        Eigen::Matrix4f view;
        Eigen::Matrix4f projectionView;
        Eigen::Vector3f camPos;
    };

    Camera(const LogicalDevice& logicalDevice);
    ~Camera();

    void SetOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
    void SetPerspectiveProjection(float fovy, float spect, float near, float far);

    vk::DescriptorSet GetVertexDescriptorSet(n32 index) const { return m_vertProjectionMatrixSet[index]; };
    vk::DescriptorSet GetFragmentDescriptorSet(n32 index) const { return m_fragProjectionMatrixSet[index]; };
    vk::DescriptorSet GetComputeDescriptorSet(n32 index) const { return m_compProjectionMatrixSet[index]; };
    vk::DescriptorSet GetParamDescriptorSet(n32 index) const { return m_uboParamSet[index]; };

    void DrawUI();

    vk::DescriptorSetLayout              GetParamDescriptorSetLayout() const { return m_paramDescriptorLayout->GetDescriptorSetLayout(); };
    const std::vector<vk::DescriptorSet> GetCombinedSets(n32 index) const { return {m_vertProjectionMatrixSet[index], m_uboParamSet[index]}; };
    vk::DescriptorSetLayout              GetVertexDescriptorLayout() const { return m_vertProjectionLayout->GetDescriptorSetLayout(); };
    vk::DescriptorSetLayout              GetFragmentDescriptorSetLayout() const { return m_fragProjectionLayout->GetDescriptorSetLayout(); };
    vk::DescriptorSetLayout              GetComputeDescriptorSetLayout() const { return m_compProjectionLayout->GetDescriptorSetLayout(); };
    vk::Buffer                           GetProjectionBuffer(n32 index) const { return m_projectionBuffers[index]->GetBuffer(); };
    Buffer&                              GetProjectionBufferHandle(n32 index) const { return *m_projectionBuffers[index]; }
    Buffer&                              GetCombinedDataBufferHandle(n32 index) const { return *m_combinedCameraDataBuffers[index]; }

    const Eigen::Matrix4f& GetProjection() const { return m_projectionMatrix; };
    const Eigen::Matrix4f& GetView() const { return m_viewMatrix; };

    Eigen::Matrix4f GetProjectionViewMatrix() const { return m_projectionMatrix * m_viewMatrix; }

    static void ExtractFrustumPlanes(const Eigen::Matrix4f& viewProjectionMatrix, std::array<Plane, 6>& planes);
    bool        IsAABBOutsidePlane(const Plane& plane, const Eigen::Vector3f& aabbMin, const Eigen::Vector3f& aabbMax) const;
    bool        IsAABBInsideFrustum(const Eigen::Vector3f& aabbMin, const Eigen::Vector3f& aabbMax) const;

    void UpdateViewMatrix(); // Function to calculate and update m_viewMatrix

    void Update();

    void SetPosition(Eigen::Vector3f position) { m_position = position; }
    void SetRotation(Eigen::Vector3f rotation) { m_rotation = rotation; }

    Eigen::Vector3f GetPosition() const { return m_position; }
    Eigen::Vector3f GetRotation() const { return m_rotation; }

    Eigen::Vector3f GetForward() const { return m_forward; }
    Eigen::Vector3f GetUp() const { return m_up; }

private:
    n32 m_index{0};

    std::vector<std::unique_ptr<Buffer>> m_projectionBuffers;
    std::vector<std::unique_ptr<Buffer>> m_paramBuffers;
    std::vector<std::unique_ptr<Buffer>> m_combinedCameraDataBuffers;

    std::unique_ptr<DescriptorPool>      m_projectionPool;
    std::unique_ptr<DescriptorSetLayout> m_vertProjectionLayout;
    std::unique_ptr<DescriptorSetLayout> m_fragProjectionLayout;
    std::unique_ptr<DescriptorSetLayout> m_compProjectionLayout;
    std::unique_ptr<DescriptorSetLayout> m_paramDescriptorLayout;

    std::vector<vk::DescriptorSet> m_vertProjectionMatrixSet;
    std::vector<vk::DescriptorSet> m_fragProjectionMatrixSet;
    std::vector<vk::DescriptorSet> m_compProjectionMatrixSet;
    std::vector<vk::DescriptorSet> m_uboParamSet;
    UboParams                      m_uboParams{};

    Eigen::Matrix4f m_projectionMatrix{};
    Eigen::Matrix4f m_viewMatrix{};
    Eigen::Vector3f m_position{};
    Eigen::Vector3f m_rotation{};
    Eigen::Vector3f m_forward{};
    Eigen::Vector3f m_up{};

    void InitDescriptorThings(const LogicalDevice& logicalDevice);

    void UpdateUBO(n32 index);
    void UpdateCombinedCameraData(n32 index);
};
} // namespace Humongous
