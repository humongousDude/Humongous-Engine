#define GLM_ENABLE_EXPERIMENTAL

#include "scene.hpp"
#include "glm/gtx/quaternion.hpp"
#include <glm/fwd.hpp>

#define MAX_NUM_JOINTS 128u

namespace Humongous
{

void Node::CalculateLocalMatrix()
{
    glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 R = glm::toMat4(rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

    localMatrix = T * R * S;
}

void Node::UpdateLocalToModelMatrix(const glm::mat4& parentWorldMatrix)
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
}
} // namespace Humongous
