#pragma once

#include "material.hpp"

#include <vector>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Humongous
{
class Mesh;
struct Skin;

struct Node
{
    ~Node();
    Node*              parent;
    n32                index;
    std::vector<Node*> children;
    glm::mat4          localMatrix{0};
    glm::mat4          localToModelMatrix{0};
    std::string        name;
    Mesh*              mesh;
    glm::vec3          translation{};
    glm::vec3          scale{1.0f};
    glm::quat          rotation{};
    Skin*              skin;
    s32                skinIndex{-1};
    b32                isMatrixSpecified{false};

    void CalculateLocalMatrix();
    void UpdateLocalToModelMatrix(const glm::mat4& parentWorldMatrix);
    void UpdateLocalToModelMatrix();
};

struct Skin
{
    std::string            name;
    Node*                  skeletonRoot = nullptr;
    std::vector<glm::mat4> jointMatrices;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<Node*>     joints;

    void UpdateJointMatrices()
    {
        if(joints.empty()) { return; }

        if(jointMatrices.size() != joints.size()) { jointMatrices.resize(joints.size()); }

        for(size_t i = 0; i < joints.size(); ++i)
        {
            Node* jointNode = joints[i];
            jointMatrices[i] = jointNode->localToModelMatrix * inverseBindMatrices[i];
        }
    }
};

} // namespace Humongous
