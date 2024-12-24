#include "abstractions/descriptor_writer.hpp"
#include "logger.hpp"
#include <camera.hpp>

#include <swapchain.hpp>

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Humongous
{

Camera::Camera(LogicalDevice* logicalDevice) { InitDescriptorThings(logicalDevice); }

Camera::~Camera() {}

void Camera::InitDescriptorThings(LogicalDevice* logicalDevice)
{
    HGINFO("Initializing descriptor things...");

    DescriptorPool::Builder builder{*logicalDevice};
    builder.SetMaxSets(40);
    builder.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20);
    builder.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 20);
    m_projectionPool = builder.Build();

    DescriptorSetLayout::Builder builder2{*logicalDevice};
    builder2.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    m_projectionLayout = builder2.build();

    DescriptorSetLayout::Builder builder3{*logicalDevice};
    builder3.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_paramLayout = builder3.build();

    m_projectionBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_projectionMatrixSet.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_uboParamSet.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_paramBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    for(int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_projectionBuffers[i] =
            std::make_unique<Buffer>(logicalDevice, SwapChain::MAX_FRAMES_IN_FLIGHT, sizeof(ProjectionUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_AUTO);
        m_projectionBuffers[i]->Map();

        auto bufInfo = m_projectionBuffers[i]->DescriptorInfo();
        DescriptorWriter(*m_projectionLayout, m_projectionPool.get()).WriteBuffer(0, &bufInfo).Build(m_projectionMatrixSet[i]);

        m_paramBuffers[i] =
            std::make_unique<Buffer>(logicalDevice, SwapChain::MAX_FRAMES_IN_FLIGHT, sizeof(UboParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_AUTO);
        m_paramBuffers[i]->Map();

        auto paramInfo = m_paramBuffers[i]->DescriptorInfo();
        DescriptorWriter(*m_paramLayout, m_projectionPool.get()).WriteBuffer(0, &paramInfo).Build(m_uboParamSet[i]);
    }
}

void Camera::UpdateUBO(n32 index, const glm::vec3& camPos)
{
    ProjectionUBO ubo{};
    ubo.projection = m_projectionMatrix;
    ubo.view = m_viewMatrix;
    ubo.cameraPos = camPos;

    m_projectionBuffers[index]->WriteToBuffer(&ubo);

    UboParams params{};
    m_paramBuffers[index]->WriteToBuffer(&params);
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
    assert(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);
    const float tanHalfFovy = tan(fovy / 2.f);
    m_projectionMatrix = glm::mat4{0.0f};
    m_projectionMatrix[0][0] = 1.f / (aspect * tanHalfFovy);
    m_projectionMatrix[1][1] = 1.f / (tanHalfFovy);
    m_projectionMatrix[2][2] = far / (far - near);
    m_projectionMatrix[2][3] = 1.f;
    m_projectionMatrix[3][2] = -(far * near) / (far - near);
}

void Camera::SetViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up)
{
    const glm::vec3 w{glm::normalize(direction)};
    const glm::vec3 u{glm::normalize(glm::cross(w, up))};
    const glm::vec3 v{glm::cross(w, u)};

    m_viewMatrix = glm::mat4{1.f};
    m_viewMatrix[0][0] = u.x;
    m_viewMatrix[1][0] = u.y;
    m_viewMatrix[2][0] = u.z;
    m_viewMatrix[0][1] = v.x;
    m_viewMatrix[1][1] = v.y;
    m_viewMatrix[2][1] = v.z;
    m_viewMatrix[0][2] = w.x;
    m_viewMatrix[1][2] = w.y;
    m_viewMatrix[2][2] = w.z;
    m_viewMatrix[3][0] = -glm::dot(u, position);
    m_viewMatrix[3][1] = -glm::dot(v, position);
    m_viewMatrix[3][2] = -glm::dot(w, position);
}

void Camera::SetViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up) { SetViewDirection(position, target - position, up); }

void Camera::SetViewYXZ(glm::vec3 position, glm::vec3 rotation)
{
    const float     c3 = glm::cos(rotation.z);
    const float     s3 = glm::sin(rotation.z);
    const float     c2 = glm::cos(rotation.x);
    const float     s2 = glm::sin(rotation.x);
    const float     c1 = glm::cos(rotation.y);
    const float     s1 = glm::sin(rotation.y);
    const glm::vec3 u{(c1 * c3 + s1 * s2 * s3), (c2 * s3), (c1 * s2 * s3 - c3 * s1)};
    const glm::vec3 v{(c3 * s1 * s2 - c1 * s3), (c2 * c3), (c1 * c3 * s2 + s1 * s3)};
    const glm::vec3 w{(c2 * s1), (-s2), (c1 * c2)};
    m_viewMatrix = glm::mat4{1.f};
    m_viewMatrix[0][0] = u.x;
    m_viewMatrix[1][0] = u.y;
    m_viewMatrix[2][0] = u.z;
    m_viewMatrix[0][1] = v.x;
    m_viewMatrix[1][1] = v.y;
    m_viewMatrix[2][1] = v.z;
    m_viewMatrix[0][2] = w.x;
    m_viewMatrix[1][2] = w.y;
    m_viewMatrix[2][2] = w.z;
    m_viewMatrix[3][0] = -glm::dot(u, position);
    m_viewMatrix[3][1] = -glm::dot(v, position);
    m_viewMatrix[3][2] = -glm::dot(w, position);
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
bool Camera::IsAABBOutsidePlane(const Plane& plane, const glm::vec3& aabbMin, const glm::vec3& aabbMax)
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
bool Camera::IsAABBInsideFrustum(const glm::vec3& aabbMin, const glm::vec3& aabbMax)
{
    std::array<Plane, 6> frustumPlanes;
    Camera::ExtractFrustumPlanes(GetVPM(), frustumPlanes);

    for(int i = 0; i < 6; i++)
    {
        if(IsAABBOutsidePlane(frustumPlanes[i], aabbMin, aabbMax))
        {
            return false; // AABB is outside the frustum
        }
    }

    return true; // AABB is inside or intersecting the frustum
}

}; // namespace Humongous
