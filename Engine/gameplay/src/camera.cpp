#include "camera.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "imgui.h"
#include "logger.hpp"
#include "swapchain.hpp"
#include "ui/widget.hpp"

namespace Humongous
{

Camera::Camera(const LogicalDevice& logicalDevice) { InitDescriptorThings(logicalDevice); }

Camera::~Camera()
{
    for(u32 i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_projectionBuffers[i].reset();
        m_paramBuffers[i].reset();
        m_combinedCameraDataBuffers[i].reset();
    }

    m_projectionPool.reset();
    m_vertProjectionLayout.reset();
    m_fragProjectionLayout.reset();
    m_compProjectionLayout.reset();
    m_paramDescriptorLayout.reset();
}

void Camera::InitDescriptorThings(const LogicalDevice& logicalDevice)
{
    HGINFO("Initializing descriptor things...");

    DescriptorPool::Builder builder{logicalDevice};
    builder.SetMaxSets(25);
    builder.AddPoolSize(vk::DescriptorType::eUniformBuffer, 25);
    m_projectionPool = builder.Build();

    DescriptorSetLayout::Builder builder2{logicalDevice};
    builder2.AddBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex);
    m_vertProjectionLayout = builder2.Build();

    DescriptorSetLayout::Builder builder3{logicalDevice};
    builder3.AddBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment);
    m_fragProjectionLayout = builder3.Build();

    DescriptorSetLayout::Builder builder4{logicalDevice};
    builder4.AddBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute);
    m_compProjectionLayout = builder4.Build();

    DescriptorSetLayout::Builder builder5{logicalDevice};
    builder5.AddBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute);
    m_paramDescriptorLayout = builder5.Build();

    m_projectionBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_vertProjectionMatrixSet.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_fragProjectionMatrixSet.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_compProjectionMatrixSet.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_uboParamSet.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_paramBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_combinedCameraDataBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    for(int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_projectionBuffers[i] =
            std::make_unique<Buffer>(logicalDevice, SwapChain::MAX_FRAMES_IN_FLIGHT, sizeof(ProjectionUBO), vk::BufferUsageFlagBits::eUniformBuffer,
                                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO);
        m_projectionBuffers[i]->Map();

        auto bufInfo = m_projectionBuffers[i]->DescriptorInfo();
        DescriptorWriter(*m_vertProjectionLayout, m_projectionPool.get()).WriteBuffer(0, &bufInfo).Build(m_vertProjectionMatrixSet[i]);
        DescriptorWriter(*m_fragProjectionLayout, m_projectionPool.get()).WriteBuffer(0, &bufInfo).Build(m_fragProjectionMatrixSet[i]);
        DescriptorWriter(*m_compProjectionLayout, m_projectionPool.get()).WriteBuffer(0, &bufInfo).Build(m_compProjectionMatrixSet[i]);

        m_paramBuffers[i] =
            std::make_unique<Buffer>(logicalDevice, SwapChain::MAX_FRAMES_IN_FLIGHT, sizeof(UboParams), vk::BufferUsageFlagBits::eUniformBuffer,
                                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO);
        m_paramBuffers[i]->Map();

        auto paramInfo = m_paramBuffers[i]->DescriptorInfo();
        DescriptorWriter(*m_paramDescriptorLayout, m_projectionPool.get()).WriteBuffer(0, &paramInfo).Build(m_uboParamSet[i]);

        m_combinedCameraDataBuffers[i] = std::make_unique<Buffer>(
            logicalDevice, SwapChain::MAX_FRAMES_IN_FLIGHT, sizeof(CombinedCameraData), vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO);
    }
}

void Camera::Update()
{
    UpdateViewMatrix();
    UpdateUBO(m_index);
    UpdateCombinedCameraData(m_index);

    m_index = (m_index + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}

void Camera::UpdateUBO(u32 index)
{
    ProjectionUBO ubo{};
    ubo.projection = m_projectionMatrix;
    ubo.invProjection = m_projectionMatrix.inverse();
    ubo.view = m_viewMatrix;
    ubo.invView = m_viewMatrix.inverse();
    ubo.projectionView = GetProjectionViewMatrix();
    ubo.cameraPos = m_position;

    m_projectionBuffers[index]->WriteToBuffer(&ubo);

    m_uboParams.camPos = m_position;
    // m_uboParams.lightDir = glm::vec4(glm::normalize(Eigen::Vector3f(1.0f, -3.0f, 1.0f)), 0.0f); // Example directional light
    // m_uboParams.exposure = 2.0f;                                  // Example exposure
    // m_uboParams.gamma = 2.2f;                                     // Standard gamma
    m_uboParams.prefilteredCubeMipLevels = static_cast<f32>(9); // Get actual mip count
    // m_uboParams.scaleIBLAmbient = 1.0f;                           // Start with no ambient scaling
    // m_uboParams.debugViewInputs = 0;   // Debugging disabled
    // m_uboParams.debugViewEquation = 0; // Debugging disabled
    m_paramBuffers[index]->WriteToBuffer(&m_uboParams);
}

void Camera::UpdateCombinedCameraData(u32 index)
{
    Camera::CombinedCameraData data;
    data.camPos = m_position;
    data.projection = m_projectionMatrix;
    data.view = m_viewMatrix;
    data.projectionView = GetProjectionViewMatrix();

    m_combinedCameraDataBuffers[index]->Map();
    m_combinedCameraDataBuffers[index]->WriteToBuffer(&data);
    m_combinedCameraDataBuffers[index]->UnMap();
}

void Camera::SetOrthographicProjection(f32 left, f32 right, f32 top, f32 bottom, f32 near, f32 far)
{
    m_projectionMatrix.setZero();

    m_projectionMatrix(0, 0) = 2.0f / (right - left);
    m_projectionMatrix(1, 1) = 2.0f / (bottom - top);
    m_projectionMatrix(2, 2) = 1.0f / (far - near);
    m_projectionMatrix(0, 3) = -(right + left) / (right - left);
    m_projectionMatrix(1, 3) = -(bottom + top) / (bottom - top);
    m_projectionMatrix(2, 3) = -near / (far - near);
    m_projectionMatrix(3, 3) = 1.0f;
}

void Camera::SetPerspectiveProjection(f32 fovy, f32 aspect, f32 near, f32 far)
{
    float f = 1.0f / std::tan(fovy / 2.0f);

    m_projectionMatrix = Eigen::Matrix4f::Zero();
    m_projectionMatrix(0, 0) = f / aspect;
    m_projectionMatrix(1, 1) = f;
    m_projectionMatrix(2, 2) = near / (near - far);
    m_projectionMatrix(2, 3) = -(near * far) / (near - far);
    m_projectionMatrix(3, 2) = 1.0f;
    m_projectionMatrix(1, 1) *= -1.0f;
}

void Camera::UpdateViewMatrix()
{
    Eigen::Vector3f forwardDir_initial = Eigen::Vector3f(0.0f, 0.0f, 1.0f);
    Eigen::Vector3f worldUp = Eigen::Vector3f(0.0f, 1.0f, 0.0f);

    Eigen::Quaternionf cameraQuaternion = Eigen::AngleAxisf(m_rotation.y(), Eigen::Vector3f::UnitY()) * // Yaw
                                          Eigen::AngleAxisf(m_rotation.x(), Eigen::Vector3f::UnitX()) * // Pitch
                                          Eigen::AngleAxisf(m_rotation.z(), Eigen::Vector3f::UnitZ());  // Roll

    m_forward = (cameraQuaternion * forwardDir_initial).normalized();
    m_up = (cameraQuaternion * worldUp).normalized();

    Eigen::Vector3f Z = m_forward;
    Eigen::Vector3f X = worldUp.cross(Z).normalized();
    Eigen::Vector3f Y = Z.cross(X).normalized();

    m_viewMatrix = Eigen::Matrix4f::Identity();
    m_viewMatrix.block<1, 3>(0, 0) = X.transpose();
    m_viewMatrix.block<1, 3>(1, 0) = Y.transpose();
    m_viewMatrix.block<1, 3>(2, 0) = Z.transpose();
    m_viewMatrix.block<3, 1>(0, 3) << -X.dot(m_position), -Y.dot(m_position), -Z.dot(m_position);
}

void Camera::ExtractFrustumPlanes(const Eigen::Matrix4f& projectionViewMatrix, std::array<Plane, 6>& frustumPlanes)
{
    Eigen::Vector4f row0 = projectionViewMatrix.row(0);
    Eigen::Vector4f row1 = projectionViewMatrix.row(1);
    Eigen::Vector4f row2 = projectionViewMatrix.row(2);
    Eigen::Vector4f row3 = projectionViewMatrix.row(3);

    frustumPlanes[0].normal = (row3 + row0).head<3>();
    frustumPlanes[0].distance = row3.w() + row0.w();

    frustumPlanes[1].normal = (row3 - row0).head<3>();
    frustumPlanes[1].distance = row3.w() - row0.w();

    frustumPlanes[2].normal = (row3 + row1).head<3>();
    frustumPlanes[2].distance = row3.w() + row1.w();

    frustumPlanes[3].normal = (row3 - row1).head<3>();
    frustumPlanes[3].distance = row3.w() - row1.w();

    frustumPlanes[4].normal = (row3 - row2).head<3>();
    frustumPlanes[4].distance = row3.w() - row2.w();

    frustumPlanes[5].normal = row2.head<3>();
    frustumPlanes[5].distance = row2.w();

    // Normalize all plane equations
    for(int i = 0; i < 6; ++i)
    {
        f32 length = frustumPlanes[i].normal.norm();
        frustumPlanes[i].normal /= length;
        frustumPlanes[i].distance /= length;
    }
}

bool Camera::IsAABBOutsidePlane(const Plane& plane, const Eigen::Vector3f& aabbMin, const Eigen::Vector3f& aabbMax) const
{
    // Find the vertex of the AABB that is furthest in the direction of the plane's normal (p-vertex)
    Eigen::Vector3f positiveVertex = aabbMin;
    if(plane.normal.x() >= 0.0f) { positiveVertex.x() = aabbMax.x(); }
    if(plane.normal.y() >= 0.0f) { positiveVertex.y() = aabbMax.y(); }
    if(plane.normal.z() >= 0.0f) { positiveVertex.z() = aabbMax.z(); }

    // If the p-vertex is on the negative side of the plane, the entire AABB is outside.
    if(plane.normal.dot(positiveVertex) + plane.distance < 0.0f)
    {
        return true; // AABB is outside
    }

    return false; // AABB is at least partially inside
}

bool Camera::IsAABBInsideFrustum(const Eigen::Vector3f& aabbMin, const Eigen::Vector3f& aabbMax) const
{
    std::array<Plane, 6> frustumPlanes;
    Camera::ExtractFrustumPlanes(GetProjectionViewMatrix(), frustumPlanes);

    for(int i = 0; i < 6; i++)
    {
        if(IsAABBOutsidePlane(frustumPlanes[i], aabbMin, aabbMax))
        {
            return false; // AABB is outside the frustum
        }
    }

    return true; // AABB is inside or intersecting the frustum
}

void Camera::DrawUI()
{
    static bool     first = true;
    static UiWidget widget{"Lighting", true, {0, 500}, {400, 500}, 0};
    if(first)
    {
        widget.Add([&]() {
            f32 lightDir[3] = {m_uboParams.lightDir.x(), m_uboParams.lightDir.y(), m_uboParams.lightDir.z()};
            ImGui::DragFloat3("Light direction", lightDir);
            m_uboParams.lightDir.x() = lightDir[0];
            m_uboParams.lightDir.y() = lightDir[1];
            m_uboParams.lightDir.z() = lightDir[2];

            ImGui::DragFloat("gamma", &m_uboParams.gamma);
            ImGui::DragFloat("exposure", &m_uboParams.exposure);
            ImGui::DragFloat("radiance", &m_uboParams.radiance);
            ImGui::DragFloat("scaleIBLAmbient", &m_uboParams.scaleIBLAmbient);
            ImGui::SliderInt("debug lighting", &m_uboParams.debugViewInputs, 0, 5);
            ImGui::SliderInt("debug equation", &m_uboParams.debugViewEquation, 0, 5);
        });

        first = false;
    }

    widget.Draw();
}
}; // namespace Humongous
