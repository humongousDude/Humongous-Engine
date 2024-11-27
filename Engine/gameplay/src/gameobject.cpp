#include <gameobject.hpp>

namespace Humongous
{

glm::mat4 TransformComponent::Mat4() const
{
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    return glm::mat4{{
                         scale.x * (c1 * c3 + s1 * s2 * s3),
                         scale.x * (c2 * s3),
                         scale.x * (c1 * s2 * s3 - c3 * s1),
                         0.0f,
                     },
                     {
                         scale.y * (c3 * s1 * s2 - c1 * s3),
                         scale.y * (c2 * c3),
                         scale.y * (c1 * c3 * s2 + s1 * s3),
                         0.0f,
                     },
                     {
                         scale.z * (c2 * s1),
                         scale.z * (-s2),
                         scale.z * (c1 * c2),
                         0.0f,
                     },
                     {translation.x, translation.y, translation.z, 1.0f}};
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

std::vector<glm::vec3> GameObject::TransformAABBToWorldSpace(const Model::Dimensions& modelBB, const glm::mat4& modelMatrix)
{
    // AABB corners in local space
    std::vector<glm::vec3> corners = {
        {modelBB.min.x, modelBB.min.y, modelBB.min.z}, // Min corner
        {modelBB.max.x, modelBB.min.y, modelBB.min.z}, {modelBB.min.x, modelBB.max.y, modelBB.min.z}, {modelBB.max.x, modelBB.max.y, modelBB.min.z},
        {modelBB.min.x, modelBB.min.y, modelBB.max.z}, {modelBB.max.x, modelBB.min.y, modelBB.max.z}, {modelBB.min.x, modelBB.max.y, modelBB.max.z},
        {modelBB.max.x, modelBB.max.y, modelBB.max.z} // Max corner
    };

    // Transform each corner to world space
    for(glm::vec3& corner: corners) { corner = glm::vec3(modelMatrix * glm::vec4(corner, 1.0f)); }

    return corners;
}
BoundingBox GameObject::ComputeWorldAABB(const std::vector<glm::vec3>& worldCorners)
{
    BoundingBox world{};
    world.min = worldCorners[0];
    world.max = worldCorners[0];
    for(const glm::vec3& corner: worldCorners)
    {
        world.min = glm::min(world.min, corner);
        world.max = glm::max(world.max, corner);
    }

    return world;
}

void GameObject::SetModel(std::shared_ptr<Model> model)
{
    this->model = model;
    auto corners = TransformAABBToWorldSpace(model->GetDimensions(), transform.Mat4());
    aabb = ComputeWorldAABB(corners);
}

void GameObject::Update()
{
    if(m_prevFrameTransform != transform)
    {
        auto corners = TransformAABBToWorldSpace(model->GetDimensions(), transform.Mat4());
        aabb = ComputeWorldAABB(corners);
    }

    m_prevFrameTransform = transform;
}

} // namespace Humongous
