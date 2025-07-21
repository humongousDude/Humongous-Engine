#pragma once

#include "defines.hpp"
#include "entity_component_system/components/entity_component.hpp"
#include <Eigen/Dense>

namespace Humongous
{

struct TransformComponent : public EntityComponent
{
public:
    Eigen::Matrix4f Mat4() const;
    Eigen::Matrix3f NormalMatrix();

    void SetTranslation(const f32& x, const f32& y, const f32& z)
    {
        translation.x() = x;
        translation.y() = y;
        translation.z() = z;

        isDirty = true;
    }

    void SetScale(const f32& x, const f32& y, const f32& z)
    {
        scale.x() = x;
        scale.y() = y;
        scale.z() = z;

        isDirty = true;
    }

    void SetRotation(const f32& x, const f32& y, const f32& z)
    {
        rotation.x() = x;
        rotation.y() = y;
        rotation.z() = z;

        isDirty = true;
    }

    void SetDirty(const b32& isDirty) { this->isDirty = isDirty; }

    Eigen::Vector3f GetTranslation() { return translation; }
    Eigen::Vector3f GetScale() { return scale; }
    Eigen::Vector3f GetRotation() { return rotation; }
    b32             IsDirty() { return isDirty; }

    bool operator==(const TransformComponent& other)
    {
        return translation == other.translation && scale == other.scale && rotation == other.rotation;
    }

private:
    Eigen::Vector3f translation{};
    Eigen::Vector3f scale = Eigen::Vector3f::Ones();
    Eigen::Vector3f rotation{};
    b32             isDirty{false};
};
} // namespace Humongous
