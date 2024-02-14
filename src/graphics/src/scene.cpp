#include <model.hpp>
#include <scene.hpp>

namespace Humongous
{

// Node
glm::mat4 Node::LocalMatrix()
{
    return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
}

glm::mat4 Node::GetMatrix()
{
    glm::mat4 m = LocalMatrix();
    Node*     p = parent;
    while(p)
    {
        m = p->LocalMatrix() * m;
        p = p->parent;
    }
    return m;
}

void Node::Update()
{
    if(mesh)
    {
        glm::mat4 m = GetMatrix();
        // memcpy(mesh->uniformBuffer.mapped, &m, sizeof(glm::mat4));
        mesh->uniformBuffer.uniformBuffer.WriteToBuffer((void*)&m, sizeof(glm::mat4));
    }

    for(auto& child: children) { child->Update(); }
}

Node::~Node()
{
    if(mesh) { delete mesh; }
    for(auto& child: children) { delete child; }
}
} // namespace Humongous
