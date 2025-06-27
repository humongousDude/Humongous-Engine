#include "entity_component_system/components/transform_component.hpp"

namespace Humongous
{
glm::mat4 TransformComponent::Mat4() const
{
    glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 R = glm::toMat4(glm::quat(glm::radians(rotation)));
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

    return T * R * S;
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
} // namespace Humongous
