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

struct Node
{
    ~Node();
    Node*              parent;
    uint32_t           index;
    std::vector<Node*> children;
    glm::mat4          matrix;
    std::string        name;
    Mesh*              mesh;
    glm::vec3          translation{};
    glm::vec3          scale{1.0f};
    glm::quat          rotation{};
    BoundingBox        bvh;
    BoundingBox        aabb;
    glm::mat4          LocalMatrix();
    glm::mat4          GetMatrix();
    void               Update();
};

} // namespace Humongous
