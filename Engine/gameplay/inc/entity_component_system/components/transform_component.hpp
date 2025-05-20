#pragma once

#include "defines.hpp"
#include "entity_component_system/components/entity_component.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

namespace Humongous
{

struct TransformComponent : public EntityComponent
{
public:
    glm::mat4 Mat4() const;
    glm::mat3 NormalMatrix();

    void SetTranslation(const f32& x, const f32& y, const f32& z)
    {
        translation.x = x;
        translation.y = y;
        translation.z = z;

        isDirty = true;
    }

    void SetScale(const f32& x, const f32& y, const f32& z)
    {
        scale.x = x;
        scale.y = y;
        scale.z = z;

        isDirty = true;
    }

    void SetRotation(const f32& x, const f32& y, const f32& z)
    {
        rotation.x = x;
        rotation.y = y;
        rotation.z = z;

        isDirty = true;
    }

    void SetDirty(const b32& isDirty) { this->isDirty = isDirty; }

    glm::vec3 GetTranslation() { return translation; }
    glm::vec3 GetScale() { return scale; }
    glm::vec3 GetRotation() { return rotation; }
    b32       IsDirty() { return isDirty; }

    bool operator==(const TransformComponent& other)
    {
        return translation == other.translation && scale == other.scale && rotation == other.rotation;
    }

private:
    glm::vec3 translation{}; // position offset;
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    glm::vec3 rotation{};
    b32       isDirty{false};
};
} // namespace Humongous
