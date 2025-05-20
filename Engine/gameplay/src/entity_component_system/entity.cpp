#include "entity_component_system/entity.hpp"
#include "scene_handler.hpp"

namespace Humongous
{

// void Entity::UpdateBoundingData()
// {
//     if(!model) { return; }
//
//     auto world = SceneHandler::GetWorld();
//
//     BoundingBox localBounds = model->GetLocalBoundingBox();
//     // glm::mat4   modelMatrix = transform.Mat4();
//     glm::mat4 modelMatrix = glm::mat4(1);
//
//     for(int i = 0; i < 8; ++i)
//     {
//         glm::vec4 worldCorner = modelMatrix * glm::vec4(localBounds.corners[i].x, localBounds.corners[i].y, localBounds.corners[i].z, 1.0f);
//         m_worldBounds.corners[i] = worldCorner;
//     }
//
//     m_worldBounds.min = glm::vec3(std::numeric_limits<float>::max());
//     m_worldBounds.max = glm::vec3(std::numeric_limits<float>::lowest());
//
//     for(const auto& corner: m_worldBounds.corners)
//     {
//         m_worldBounds.min = glm::min(m_worldBounds.min, glm::vec3(corner));
//         m_worldBounds.max = glm::max(m_worldBounds.max, glm::vec3(corner));
//     }
//
//     m_worldBounds.valid = true;
// }

} // namespace Humongous
