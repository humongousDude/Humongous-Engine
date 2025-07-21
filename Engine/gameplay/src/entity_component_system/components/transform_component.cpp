#include "entity_component_system/components/transform_component.hpp"

namespace Humongous
{
Eigen::Matrix4f TransformComponent::Mat4() const
{
    Eigen::Affine3f transform = Eigen::Affine3f::Identity();

    transform.translate(translation);

    Eigen::Quaternionf q_x(Eigen::AngleAxisf(rotation.x(), Eigen::Vector3f::UnitX()));
    Eigen::Quaternionf q_y(Eigen::AngleAxisf(rotation.y(), Eigen::Vector3f::UnitY()));
    Eigen::Quaternionf q_z(Eigen::AngleAxisf(rotation.z(), Eigen::Vector3f::UnitZ()));

    Eigen::Quaternionf finalRotation = q_x * q_y * q_z;
    transform.rotate(finalRotation);

    transform.scale(scale);

    return transform.matrix();
}

Eigen::Matrix3f TransformComponent::NormalMatrix()
{
    Eigen::AngleAxisf pitch(rotation.x(), Eigen::Vector3f::UnitX());
    Eigen::AngleAxisf yaw(rotation.y(), Eigen::Vector3f::UnitY());
    Eigen::AngleAxisf roll(rotation.z(), Eigen::Vector3f::UnitZ());

    Eigen::Matrix3f rotationMatrix = (yaw * pitch * roll).toRotationMatrix();

    Eigen::Matrix3f scaleMatrix = Eigen::DiagonalMatrix<float, 3>(scale);
    Eigen::Matrix3f model3x3 = rotationMatrix * scaleMatrix;

    return model3x3.inverse().transpose();
}
} // namespace Humongous
