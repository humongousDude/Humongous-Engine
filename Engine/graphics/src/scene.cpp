#include "logger.hpp"
#define GLM_ENABLE_EXPERIMENTAL

#include "glm/gtx/quaternion.hpp"
#include "scene.hpp"
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
    CalculateLocalMatrix();
    localToModelMatrix = parentWorldMatrix * localMatrix;

    // HGINFO("CHILD NODE: '%s'", name.c_str());
    // HGINFO("  Parent Matrix (in):");
    // HGINFO("    %f, %f, %f, %f", parentWorldMatrix[0][0], parentWorldMatrix[1][0], parentWorldMatrix[2][0], parentWorldMatrix[3][0]);
    // HGINFO("    %f, %f, %f, %f", parentWorldMatrix[0][1], parentWorldMatrix[1][1], parentWorldMatrix[2][1], parentWorldMatrix[3][1]);
    // HGINFO("    %f, %f, %f, %f", parentWorldMatrix[0][2], parentWorldMatrix[1][2], parentWorldMatrix[2][2], parentWorldMatrix[3][2]);
    // HGINFO("    %f, %f, %f, %f", parentWorldMatrix[0][3], parentWorldMatrix[1][3], parentWorldMatrix[2][3], parentWorldMatrix[3][3]);
    //
    // HGINFO("  Local Matrix:");
    // HGINFO("    %f, %f, %f, %f", localMatrix[0][0], localMatrix[1][0], localMatrix[2][0], localMatrix[3][0]);
    // HGINFO("    %f, %f, %f, %f", localMatrix[0][1], localMatrix[1][1], localMatrix[2][1], localMatrix[3][1]);
    // HGINFO("    %f, %f, %f, %f", localMatrix[0][2], localMatrix[1][2], localMatrix[2][2], localMatrix[3][2]);
    // HGINFO("    %f, %f, %f, %f", localMatrix[0][3], localMatrix[1][3], localMatrix[2][3], localMatrix[3][3]);
    //
    // HGINFO("  Result localToModelMatrix (out):");
    // HGINFO("    %f, %f, %f, %f", localToModelMatrix[0][0], localToModelMatrix[1][0], localToModelMatrix[2][0], localToModelMatrix[3][0]);
    // HGINFO("    %f, %f, %f, %f", localToModelMatrix[0][1], localToModelMatrix[1][1], localToModelMatrix[2][1], localToModelMatrix[3][1]);
    // HGINFO("    %f, %f, %f, %f", localToModelMatrix[0][2], localToModelMatrix[1][2], localToModelMatrix[2][2], localToModelMatrix[3][2]);
    // HGINFO("    %f, %f, %f, %f", localToModelMatrix[0][3], localToModelMatrix[1][3], localToModelMatrix[2][3], localToModelMatrix[3][3]);
    //
    for(auto& child: children) { child->UpdateLocalToModelMatrix(localToModelMatrix); }
}

void Node::UpdateLocalToModelMatrix()
{
    CalculateLocalMatrix();
    localToModelMatrix = localMatrix;

    // HGINFO("CHILD NODE: '%s', No parent", name.c_str());

    // HGINFO("  Local Matrix:");
    // HGINFO("    %f, %f, %f, %f", localMatrix[0][0], localMatrix[1][0], localMatrix[2][0], localMatrix[3][0]);
    // HGINFO("    %f, %f, %f, %f", localMatrix[0][1], localMatrix[1][1], localMatrix[2][1], localMatrix[3][1]);
    // HGINFO("    %f, %f, %f, %f", localMatrix[0][2], localMatrix[1][2], localMatrix[2][2], localMatrix[3][2]);
    // HGINFO("    %f, %f, %f, %f", localMatrix[0][3], localMatrix[1][3], localMatrix[2][3], localMatrix[3][3]);
    //
    // HGINFO("  Result localToModelMatrix (out):");
    // HGINFO("    %f, %f, %f, %f", localToModelMatrix[0][0], localToModelMatrix[1][0], localToModelMatrix[2][0], localToModelMatrix[3][0]);
    // HGINFO("    %f, %f, %f, %f", localToModelMatrix[0][1], localToModelMatrix[1][1], localToModelMatrix[2][1], localToModelMatrix[3][1]);
    // HGINFO("    %f, %f, %f, %f", localToModelMatrix[0][2], localToModelMatrix[1][2], localToModelMatrix[2][2], localToModelMatrix[3][2]);
    // HGINFO("    %f, %f, %f, %f", localToModelMatrix[0][3], localToModelMatrix[1][3], localToModelMatrix[2][3], localToModelMatrix[3][3]);

    for(auto& child: children) { child->UpdateLocalToModelMatrix(localToModelMatrix); }
}

Node::~Node()
{
    for(auto& child: children) { delete child; }
}
} // namespace Humongous
