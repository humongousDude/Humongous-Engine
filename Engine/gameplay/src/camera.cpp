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

void Camera::UpdateUBO(u32 index, const glm::vec3& camPos)
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

std::vector<glm::vec4> Camera::CalculateFrustumPlanes()
{
    std::vector<glm::vec4> planes(6);

    auto viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;

    // Extract the frustum planes from the view-projection matrix
    planes[0] = glm::row(viewProjectionMatrix, 3) + glm::row(viewProjectionMatrix, 0); // Left plane
    planes[1] = glm::row(viewProjectionMatrix, 3) - glm::row(viewProjectionMatrix, 0); // Right plane
    planes[2] = glm::row(viewProjectionMatrix, 3) + glm::row(viewProjectionMatrix, 1); // Bottom plane
    planes[3] = glm::row(viewProjectionMatrix, 3) - glm::row(viewProjectionMatrix, 1); // Top plane
    planes[4] = glm::row(viewProjectionMatrix, 3) + glm::row(viewProjectionMatrix, 2); // Near plane
    planes[5] = glm::row(viewProjectionMatrix, 3) - glm::row(viewProjectionMatrix, 2); // Far plane

    // Normalize the planes
    for(auto& plane: planes)
    {
        float length = glm::length(glm::vec3(plane));
        plane /= length;
    }

    return planes;
}

bool Camera::IsAABBInFrustum(const glm::mat4& AABB, const std::vector<glm::vec4>& frustumPlanes)
{
    for(const auto& plane: frustumPlanes)
    {
        glm::vec3 normal(plane);
        glm::vec3 center(AABB[3]);                             // Assuming the center of AABB is in the fourth column of the matrix
        glm::vec3 extents(AABB[0][0], AABB[1][1], AABB[2][2]); // Assuming the extents are in the diagonal

        // Calculate the distance between the AABB center and the plane
        float distance = glm::dot(normal, center) + plane.w;

        // Calculate the projected extent along the normal direction
        float projectedExtent = glm::dot(glm::abs(normal), extents);

        // If the AABB is entirely on one side of the plane, it's outside the frustum
        if(distance < -projectedExtent) { return false; }
    }

    return true; // AABB intersects all frustum planes
}

}; // namespace Humongous
