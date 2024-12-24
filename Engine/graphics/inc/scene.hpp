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
    Node*              m_parent;
    n32                m_index;
    std::vector<Node*> m_children;
    glm::mat4          m_matrix;
    std::string        m_name;
    Mesh*              m_mesh;
    glm::vec3          m_translation{};
    glm::vec3          m_scale{1.0f};
    glm::quat          m_rotation{};
    BoundingBox        m_bvh;
    BoundingBox        m_aabb;
    glm::mat4          LocalMatrix();
    glm::mat4          GetMatrix();
    void               Update();
};

} // namespace Humongous
