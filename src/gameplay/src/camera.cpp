#include "abstractions/descriptor_writer.hpp"
#include "logger.hpp"
#include <camera.hpp>

#include <swapchain.hpp>
#include <vulkan/vk_enum_string_helper.h>

namespace Humongous
{

Camera::Camera(LogicalDevice* logicalDevice) { InitDescriptorThings(logicalDevice); }

Camera::~Camera() { m_cubeMap->Destroy(); }

void Camera::InitDescriptorThings(LogicalDevice* logicalDevice)
{
    HGINFO("Initializing descriptor things...");

    DescriptorPool::Builder builder{*logicalDevice};
    builder.SetMaxSets(20);
    builder.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3);
    builder.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 9);
    m_projectionPool = builder.Build();

    DescriptorSetLayout::Builder builder2{*logicalDevice};
    builder2.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    builder2.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_projectionLayout = builder2.build();

    DescriptorSetLayout::Builder builder69{*logicalDevice};
    builder69.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_cubeMapLayout = builder69.build();

    m_projectionBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_projectionMatrixSet.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    HGDEBUG("Looping for camera sets...");

    for(int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_projectionBuffers[i] =
            std::make_unique<Buffer>(logicalDevice, SwapChain::MAX_FRAMES_IN_FLIGHT, sizeof(ProjectionUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_AUTO);

        m_projectionBuffers[i]->Map();

        auto bufInfo = m_projectionBuffers[i]->DescriptorInfo();
        DescriptorWriter(*m_projectionLayout, m_projectionPool.get()).WriteBuffer(0, &bufInfo).Build(m_projectionMatrixSet[i]);
    }

    HGDEBUG("done");

    m_cubeMap = std::make_unique<Texture>(logicalDevice, "textures/papermill.ktx", Texture::ImageType::CUBEMAP);

    if(m_cubeMap->GetRawImageHandle() == VK_NULL_HANDLE) { HGERROR("Failed to create cubemap image"); }
    if(m_cubeMap->GetRawImageViewHandle() == VK_NULL_HANDLE) { HGERROR("Failed to create cubemap image view"); }
    if(m_cubeMap->GetRawSamplerHandle() == VK_NULL_HANDLE) { HGERROR("Failed to create cubemap sampler"); }

    HGDEBUG("Cubemap image layout is %s", string_VkImageLayout(m_cubeMap->GetRawImageLayout()));

    auto imgInfo = m_cubeMap->GetDescriptorInfo();
    if(!DescriptorWriter(*m_cubeMapLayout, m_projectionPool.get()).WriteImage(0, &imgInfo).Build(m_cubeMapSet)) { HGERROR("i shit"); }

    HGINFO("Descriptor things initialized.");
}

void Camera::UpdateUBO(u32 index)
{
    ProjectionUBO ubo{};
    ubo.projection = m_projectionMatrix;
    ubo.view = m_viewMatrix;

    m_projectionBuffers[index]->WriteToBuffer(&ubo);
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

}; // namespace Humongous
