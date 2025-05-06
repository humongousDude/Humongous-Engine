#include "abstractions/descriptor_writer.hpp"
#include "imgui.h"
#include "logger.hpp"
#include "ui/widget.hpp"
#include <camera.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <swapchain.hpp>

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Humongous
{

Camera::Camera(LogicalDevice* logicalDevice) : m_position(0), m_rotation(0) { InitDescriptorThings(logicalDevice); }

Camera::~Camera() {}

void Camera::InitDescriptorThings(LogicalDevice* logicalDevice)
{
    HGINFO("Initializing descriptor things...");

    DescriptorPool::Builder builder{*logicalDevice};
    builder.SetMaxSets(6);
    builder.AddPoolSize(vk::DescriptorType::eUniformBuffer, 6);
    m_projectionPool = builder.Build();

    DescriptorSetLayout::Builder builder2{*logicalDevice};
    builder2.addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex);
    m_projectionDescriptorLayout = builder2.Build();

    DescriptorSetLayout::Builder builder3{*logicalDevice};
    builder3.addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment);
    m_paramDescriptorLayout = builder3.Build();

    m_projectionBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_projectionMatrixSet.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
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
        DescriptorWriter(*m_projectionDescriptorLayout, m_projectionPool.get()).WriteBuffer(0, &bufInfo).Build(m_projectionMatrixSet[i]);

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

void Camera::UpdateUBO(n32 index)
{
    ProjectionUBO ubo{};
    ubo.projection = m_projectionMatrix;
    ubo.view = m_viewMatrix;
    ubo.projectionView = GetProjectionViewMatrix();
    ubo.cameraPos = m_position;

    m_projectionBuffers[index]->WriteToBuffer(&ubo);

    m_uboParams.camPos = m_position;
    // m_uboParams.lightDir = glm::vec4(glm::normalize(glm::vec3(1.0f, -3.0f, 1.0f)), 0.0f); // Example directional light
    // m_uboParams.exposure = 2.0f;                                  // Example exposure
    // m_uboParams.gamma = 2.2f;                                     // Standard gamma
    m_uboParams.prefilteredCubeMipLevels = static_cast<float>(9); // Get actual mip count
    // m_uboParams.scaleIBLAmbient = 1.0f;                           // Start with no ambient scaling
    // m_uboParams.debugViewInputs = 0;   // Debugging disabled
    // m_uboParams.debugViewEquation = 0; // Debugging disabled
    m_paramBuffers[index]->WriteToBuffer(&m_uboParams);
}

void Camera::UpdateCombinedCameraData(n32 index)
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

void Camera::SetOrthographicProjection(float left, float right, float top, float bottom, float near, float far)
{
    m_projectionMatrix = glm::mat4{1.0f};
    m_projectionMatrix[0][0] = 2.f / (right - left);
    m_projectionMatrix[1][1] = 2.f / (bottom - top);
    m_projectionMatrix[2][2] = 1.f / (far - near);
    m_projectionMatrix[3][0] = -(right + left) / (right - left);
    m_projectionMatrix[3][1] = -(bottom + top) / (bottom - top);
    m_projectionMatrix[3][2] = -near / (far - near);
}

void Camera::SetPerspectiveProjection(float fovy, float aspect, float near, float far)
{
    m_projectionMatrix = glm::perspectiveRH_NO(fovy, aspect, near, far);
}
void Camera::UpdateViewMatrix()
{
    glm::vec3 forwardDir = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::mat4 cameraRotationMatrix = glm::mat4(1.0f);
    cameraRotationMatrix = glm::rotate(cameraRotationMatrix, m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw
    cameraRotationMatrix = glm::rotate(cameraRotationMatrix, m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch
    cameraRotationMatrix = glm::rotate(cameraRotationMatrix, m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Roll
    forwardDir = glm::normalize(glm::vec3(cameraRotationMatrix * glm::vec4(forwardDir, 0.0f)));

    glm::vec3 target = m_position + forwardDir;

    m_viewMatrix = glm::lookAtRH(m_position, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::ExtractFrustumPlanes(const glm::mat4& projectionViewMatrix, std::array<Plane, 6>& frustumPlanes)
{
    // Extract rows from the matrix
    glm::vec4 row0 = glm::row(projectionViewMatrix, 0); // First row
    glm::vec4 row1 = glm::row(projectionViewMatrix, 1); // Second row
    glm::vec4 row2 = glm::row(projectionViewMatrix, 2); // Third row
    glm::vec4 row3 = glm::row(projectionViewMatrix, 3); // Fourth row

    // Left plane
    glm::vec4 left = row3 + row0;
    frustumPlanes[0].normal = glm::vec3(left.x, left.y, left.z);
    frustumPlanes[0].distance = left.w;

    // Right plane
    glm::vec4 right = row3 - row0;
    frustumPlanes[1].normal = glm::vec3(right.x, right.y, right.z);
    frustumPlanes[1].distance = right.w;

    // Bottom plane
    glm::vec4 bottom = row3 + row1;
    frustumPlanes[2].normal = glm::vec3(bottom.x, bottom.y, bottom.z);
    frustumPlanes[2].distance = bottom.w;

    // Top plane
    glm::vec4 top = row3 - row1;
    frustumPlanes[3].normal = glm::vec3(top.x, top.y, top.z);
    frustumPlanes[3].distance = top.w;

    // Near plane
    glm::vec4 nearPlane = row3 + row2;
    frustumPlanes[4].normal = glm::vec3(nearPlane.x, nearPlane.y, nearPlane.z);
    frustumPlanes[4].distance = nearPlane.w;

    // Far plane
    glm::vec4 farPlane = row3 - row2;
    frustumPlanes[5].normal = glm::vec3(farPlane.x, farPlane.y, farPlane.z);
    frustumPlanes[5].distance = farPlane.w;

    // Normalize planes
    for(int i = 0; i < 6; ++i)
    {
        float length = glm::length(frustumPlanes[i].normal);
        frustumPlanes[i].normal /= length;
        frustumPlanes[i].distance /= length;
    }
}

// Check if an AABB is outside a single frustum plane
bool Camera::IsAABBOutsidePlane(const Plane& plane, const glm::vec3& aabbMin, const glm::vec3& aabbMax) const
{
    // Calculate the positive and negative vertices relative to the plane
    glm::vec3 positiveVertex = aabbMin;
    glm::vec3 negativeVertex = aabbMax;

    if(plane.normal.x >= 0)
    {
        positiveVertex.x = aabbMax.x;
        negativeVertex.x = aabbMin.x;
    }
    if(plane.normal.y >= 0)
    {
        positiveVertex.y = aabbMax.y;
        negativeVertex.y = aabbMin.y;
    }
    if(plane.normal.z >= 0)
    {
        positiveVertex.z = aabbMax.z;
        negativeVertex.z = aabbMin.z;
    }
    if(glm::dot(plane.normal, positiveVertex) + plane.distance < 0)
    {
        return true; // AABB is outside
    }

    return false; // AABB is at least partially inside
}

// Check if an AABB is inside the frustum
bool Camera::IsAABBInsideFrustum(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const
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
            f32 lightDir[3] = {m_uboParams.lightDir.x, m_uboParams.lightDir.y, m_uboParams.lightDir.z};
            ImGui::DragFloat3("Light direction", lightDir);
            m_uboParams.lightDir.x = lightDir[0];
            m_uboParams.lightDir.y = lightDir[1];
            m_uboParams.lightDir.z = lightDir[2];

            ImGui::DragFloat("gamma", &m_uboParams.gamma);
            ImGui::DragFloat("exposure", &m_uboParams.exposure);
            ImGui::DragFloat("scaleIBLAmbient", &m_uboParams.scaleIBLAmbient);
            ImGui::SliderFloat("debug lighting", &m_uboParams.debugViewInputs, 0, 6);
            ImGui::SliderFloat("debug equation", &m_uboParams.debugViewEquation, 0, 5);
        });

        first = false;
    }

    widget.Draw();
}
}; // namespace Humongous
