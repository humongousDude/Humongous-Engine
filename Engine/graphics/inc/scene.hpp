#pragma once

#include "material.hpp"

#include <vector>

namespace Humongous
{
class Mesh;
struct Skin;

struct Node
{
    ~Node();
    Node*              parent;
    u32                index;
    std::vector<Node*> children;
    Eigen::Matrix4f    localMatrix = Eigen::Matrix4f::Zero();
    Eigen::Matrix4f    localToModelMatrix = Eigen::Matrix4f::Zero();
    std::string        name;
    Mesh*              mesh;
    Eigen::Vector3f    translation{};
    Eigen::Vector3f    scale = Eigen::Vector3f::Ones();
    Eigen::Quaternionf rotation{};
    Skin*              skin;
    s32                skinIndex{-1};
    b32                isMatrixSpecified{false};

    void CalculateLocalMatrix();
    void UpdateLocalToModelMatrix(const Eigen::Matrix4f& parentWorldMatrix);
    void UpdateLocalToModelMatrix();
};

struct Skin
{
    std::string                  name;
    Node*                        skeletonRoot = nullptr;
    std::vector<Eigen::Matrix4f> jointMatrices;
    std::vector<Eigen::Matrix4f> inverseBindMatrices;
    std::vector<Node*>           joints;

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
