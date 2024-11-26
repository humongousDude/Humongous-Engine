#define GLM_ENABLE_EXPERIMENTAL

#include "glm/gtx/quaternion.hpp"
#include <glm/fwd.hpp>
#include <model.hpp>
#include <scene.hpp>

namespace Humongous
{

// Node
glm::mat4 Node::LocalMatrix()
{
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::scale(transform, m_scale);           // Scale
    transform *= glm::toMat4(m_rotation);                 // Rotation (quaternion assumed)
    transform = glm::translate(transform, m_translation); // Translation
    transform *= m_matrix;                                // Additional transformation
    return transform;
}

glm::mat4 Node::GetMatrix()
{
    glm::mat4 m = LocalMatrix();
    Node*     p = m_parent;
    while(p)
    {
        m = p->LocalMatrix() * m;
        p = p->m_parent;
    }
    return m;
}

void Node::Update()
{
    if(m_mesh)
    {
        glm::mat4 m = GetMatrix();
        m_mesh->m_uniformBuffer.uniformBuffer.WriteToBuffer((void*)&m, sizeof(glm::mat4));
    }

    for(auto& child: m_children) { child->Update(); }
}

Node::~Node()
{
    if(m_mesh) { delete m_mesh; }
    for(auto& child: m_children) { delete child; }
}
} // namespace Humongous
