#include <gameobject.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

namespace Humongous
{

glm::mat4 TransformComponent::Mat4() const
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, translation);
    glm::quat rotationQuat = glm::quat(glm::radians(rotation));
    model = model * glm::toMat4(rotationQuat);
    model = glm::scale(model, scale);

    return model;
}

glm::mat3 TransformComponent::NormalMatrix()
{
    const float     c3 = glm::cos(rotation.z);
    const float     s3 = glm::sin(rotation.z);
    const float     c2 = glm::cos(rotation.x);
    const float     s2 = glm::sin(rotation.x);
    const float     c1 = glm::cos(rotation.y);
    const float     s1 = glm::sin(rotation.y);
    const glm::vec3 invScale = 1.0f / scale;

    return glm::mat3{{
                         invScale.x * (c1 * c3 + s1 * s2 * s3),
                         invScale.x * (c2 * s3),
                         invScale.x * (c1 * s2 * s3 - c3 * s1),
                     },
                     {
                         invScale.y * (c3 * s1 * s2 - c1 * s3),
                         invScale.y * (c2 * c3),
                         invScale.y * (c1 * c3 * s2 + s1 * s3),
                     },
                     {
                         invScale.z * (c2 * s1),
                         invScale.z * (-s2),
                         invScale.z * (c1 * c2),
                     }};
}

void GameObject::SetModel(std::shared_ptr<Model> model)
{
    this->model = model;
    Update();
}

void GameObject::Update()
{
    UpdateBoundingData();
    m_prevFrameTransform = transform;
}

void GameObject::UpdateBoundingData()
{
    if(!model) { return; }
    BoundingBox localBounds = model->GetLocalBoundingBox();
    glm::mat4   modelMatrix = transform.Mat4();

    for(int i = 0; i < 8; ++i)
    {
        glm::vec4 worldCorner = modelMatrix * glm::vec4(localBounds.corners[i].x, localBounds.corners[i].y, localBounds.corners[i].z, 1.0f);
        m_worldBounds.corners[i] = worldCorner;
    }

    m_worldBounds.min = glm::vec3(std::numeric_limits<float>::max());
    m_worldBounds.max = glm::vec3(std::numeric_limits<float>::lowest());

    for(const auto& corner: m_worldBounds.corners)
    {
        m_worldBounds.min = glm::min(m_worldBounds.min, glm::vec3(corner));
        m_worldBounds.max = glm::max(m_worldBounds.max, glm::vec3(corner));
    }

    m_worldBounds.valid = true;
}

} // namespace Humongous
