#pragma once

#include "defines.hpp"
#include "material.hpp"
#include "model.hpp"

// libs

#include <memory>

namespace Humongous
{

class Entity
{
public:
    struct UpdateData
    {
        const glm::mat4& pvm;
        const n16&       screenWidth = 800;
        const n16&       screenHeight = 600;
    };

    // Do not call outside World::AddEntity
    Entity(n32 objId) : m_id{objId} {};

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&&) = default;
    Entity& operator=(Entity&&) = default;

    const n32 GetId() const { return m_id; };

    // BoundingBox GetBoundingBox() const { return m_worldBounds; }
    //
    // std::shared_ptr<Model> model{};
    //
    // std::string name;

private:
    id_t m_id;

    BoundingBox m_worldBounds{};
};

} // namespace Humongous
