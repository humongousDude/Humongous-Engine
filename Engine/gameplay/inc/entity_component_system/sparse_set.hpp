#pragma once

#include "asserts.hpp"
#include "defines.hpp"
#include "entity_component_system/components/entity_component.hpp"
#include "logger.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace Humongous
{
using EntityID = n32;
constexpr EntityID INVALID_ENTITY = std::numeric_limits<n32>().max();
constexpr n32      MAX_ENTITIES = 100000;

// SparseSet template
template <typename T> class SparseSet
{
public:
    SparseSet() { std::fill(m_sparse.begin(), m_sparse.end(), INVALID_ENTITY); }

    void Add(EntityID entity, const T& component)
    {
        HGASSERT(entity < MAX_ENTITIES);

        const b32 inherits = std::is_base_of<EntityComponent, T>::value;
        HGASSERT(inherits && "Class must inherit from EntityComponent");

        if(m_sparse[entity] != INVALID_ENTITY)
        {
            HGTRACE("Add: Entity %i already has component (m_sparse[%i] = %i). Skipping.", entity, entity, m_sparse[entity]);
            return;
        }

        std::size_t denseIndex = m_data.size();
        HGTRACE("Add: Entity %i adding component. Setting m_sparse[%i] from %i to %zu (dense index).", entity, entity, m_sparse[entity],
                denseIndex);
        m_sparse[entity] = denseIndex;
        m_data.push_back(component);
        m_dense.push_back(entity); // Remember m_dense stores entity IDs
        HGTRACE("Add: Entity %i component added. m_sparse[%i] is now %i", entity, entity, m_sparse[entity]);
    }

    void Remove(EntityID entity)
    {
        HGASSERT(entity < MAX_ENTITIES);
        std::size_t indexToRemove = m_sparse[entity];

        if(indexToRemove == INVALID_ENTITY)
        {
            HGTRACE("Remove: Entity %i did not have component (m_sparse[%i] = %i). Skipping.", entity, entity, m_sparse[entity]);
            return;
        }

        HGTRACE("Remove: Entity %i removing component at dense index %zu. Current m_sparse[%i] is %i", entity, indexToRemove, entity,
                m_sparse[entity]);

        EntityID lastEntity = m_dense.back();

        if(indexToRemove != m_data.size() - 1)
        {
            HGTRACE("Remove: Swapping component for entity %i (dense index %zu) with last entity %i (dense index %zu)", entity, indexToRemove,
                    lastEntity, m_data.size() - 1);

            m_data[indexToRemove] = std::move(m_data.back()); // Use std::move
            m_dense[indexToRemove] = lastEntity;

            HGTRACE("Remove: Updating m_sparse[%i] (last entity) from %i to %zu", lastEntity, m_sparse[lastEntity], indexToRemove);
            m_sparse[lastEntity] = indexToRemove;
        }
        else { HGTRACE("Remove: Removing last component for entity %i at dense index %zu", entity, indexToRemove); }

        HGTRACE("Remove: Invalidating m_sparse[%i] by setting to %i", entity, INVALID_ENTITY);
        m_sparse[entity] = INVALID_ENTITY;

        m_data.pop_back();
        m_dense.pop_back();

        HGTRACE("Remove: Entity %i component removed. m_sparse[%i] is now %i. Total dense size: %zu", entity, entity, m_sparse[entity],
                m_data.size());
    }

    T* Get(EntityID entity)
    {
        HGASSERT(entity < MAX_ENTITIES);

        std::size_t index = m_sparse[entity];
        return (index != INVALID_ENTITY) ? &m_data[index] : nullptr;
    }

    bool Has(EntityID entity) const
    {
        HGASSERT(entity < MAX_ENTITIES);
        return m_sparse[entity] != INVALID_ENTITY;
    }

    void Print() const
    {
        for(std::size_t i = 0; i < m_dense.size(); ++i) { HGTRACE("Entity: %i, %f, %f", m_dense[i], m_data[i].x, m_data[i].y); }
    }

    std::array<EntityID, MAX_ENTITIES>& GetSparse() { return m_sparse; }
    std::vector<EntityID>&              GetDense() { return m_dense; }
    std::vector<T>&                     GetData() { return m_data; }

private:
    std::array<EntityID, MAX_ENTITIES> m_sparse;
    std::vector<EntityID>              m_dense;
    std::vector<T>                     m_data;
};
} // namespace Humongous
