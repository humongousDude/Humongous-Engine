#include "scene.hpp"
#include "model.hpp"

#define MAX_NUM_JOINTS 128u

namespace Humongous
{

void Node::CalculateLocalMatrix()
{
    Eigen::Affine3f localTransform = Eigen::Affine3f::Identity();
    localTransform.translate(translation);
    localTransform.rotate(rotation);
    localTransform.scale(scale);
    localMatrix = localTransform.matrix();
}

void Node::UpdateLocalToModelMatrix(const Eigen::Matrix4f& parentWorldMatrix)
{
    if(!isMatrixSpecified) { CalculateLocalMatrix(); }

    localToModelMatrix = parentWorldMatrix * localMatrix;

    for(auto& child: children) { child->UpdateLocalToModelMatrix(localToModelMatrix); }
}

void Node::UpdateLocalToModelMatrix()
{
    if(!isMatrixSpecified) { CalculateLocalMatrix(); }
    localToModelMatrix = localMatrix;

    for(auto& child: children) { child->UpdateLocalToModelMatrix(localToModelMatrix); }
}

Node::~Node()
{
    for(auto& child: children) { delete child; }
    mesh.reset();
}

} // namespace Humongous
