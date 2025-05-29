#define GLM_ENABLE_EXPERIMENTAL

#include "glm/gtx/quaternion.hpp"
#include <glm/fwd.hpp>
#include <model.hpp>
#include <scene.hpp>

namespace Humongous
{

// Node
glm::mat4 Node::LocalMatrix() const
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, translation);
    model = glm::scale(model, scale);
    model = model * glm::toMat4(rotation);
    return model;
}

glm::mat4 Node::GetMatrix() const
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
    // if(mesh)
    // {
    // glm::mat4 m = GetMatrix();
    // mesh->uniformBuffer.uniformBuffer.WriteToBuffer((void*)&m, sizeof(glm::mat4));
    // }

    // for(auto& child: children) { child->Update(); }
}

Node::~Node()
{
    if(mesh) { delete mesh; }
    for(auto& child: children) { delete child; }
}
} // namespace Humongous
