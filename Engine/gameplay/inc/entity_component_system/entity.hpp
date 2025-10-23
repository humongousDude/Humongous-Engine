#pragma once

#include "defines.hpp"
#include "material.hpp"

namespace Humongous
{

class Entity
{
public:
    // Do not call outside World::AddEntity
    Entity(u32 objId) : m_id{objId} {};

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&&) = default;
    Entity& operator=(Entity&&) = default;

    u32 GetId() const { return m_id; };

private:
    u32 m_id;

    BoundingBox m_worldBounds{};
};

} // namespace Humongous
