#pragma once

#include "defines.hpp"
#include "entity_component_system/components/model_component.hpp"
#include "entity_component_system/components/transform_component.hpp"
#include "entity_component_system/sparse_set.hpp"
#include "logger.hpp"
#include "material.hpp"
#include "non_copyable.hpp"
#include "resource_manager.hpp"

namespace Humongous
{

class World : NonCopyable
{
public:
    using id_t = Humongous::EntityID;

    World() {}
    ~World() {}

    id_t CreateEntity()
    {
        id_t               id = m_nextEntity++;
        TransformComponent tc;
        tc.SetDirty(true);
        m_transforms.Add(id, tc);

        NameComponent nc;
        nc.name = "Entity " + std::to_string(id);
        m_names.Add(id, nc);

        return id;
    }

    void DestroyEntity(const id_t& entityId)
    {
        m_transforms.Remove(entityId);
        m_models.Remove(entityId);
        m_worldBoundingBoxes.Remove(entityId);
    }

    template <typename T, typename... Args> T* AddComponent(id_t entity, Args&&... args)
    {
        SparseSet<T>& storage = GetComponentStorage<T>();
        storage.Add(entity, T(std::forward<Args>(args)...));
        T* component = storage.Get(entity);
        if(!component)
        {
            HGFATAL("FAILED TO ADD COMPONENT SOMEHOW");
            return nullptr;
        }

        if constexpr(std::is_same_v<T, ModelComponent>)
        {
            if(!GetComponent<BoundingBox>(entity)) { AddComponent<BoundingBox>(entity); }
            TransformComponent* transform = GetComponent<TransformComponent>(entity);
            if(transform) { transform->SetDirty(true); }
        }
        if constexpr(std::is_same_v<T, TransformComponent>)
        {
            if(component) { static_cast<TransformComponent*>(component)->SetDirty(true); }
        }

        return component;
    }

    template <typename T> void RemoveComponent(id_t entity)
    {
        if constexpr(std::is_same_v<T, TransformComponent>)
        {
            HGERROR("TransformComponent cannot be removed. It is a requirment for all entities");
            return;
        }

        if constexpr(std::is_same_v<T, NameComponent>)
        {
            HGERROR("NameComponent cannot be removed. It is a requirment for all entites");
            return;
        }

        SparseSet<T>& storage = GetComponentStorage<T>();
        storage.Remove(entity);

        if constexpr(std::is_same_v<T, ModelComponent>)
        {
            RemoveComponent<BoundingBox>(entity); // If model is removed, BB is no longer relevant
        }
    }

    template <typename T> T* GetComponent(id_t entity)
    {
        SparseSet<T>& storage = GetComponentStorage<T>();
        return storage.Get(entity);
    }

    template <typename T> bool HasComponent(id_t entity) const
    {
        const SparseSet<T>& storage = const_cast<World*>(this)->GetComponentStorage<T>(); // Need to adapt for const
        return storage.Has(entity);
    }

    void BoundingVolumeUpdateSystem()
    {
        for(Humongous::EntityID entity: m_worldBoundingBoxes.GetDense())
        {
            TransformComponent* transform = GetComponent<TransformComponent>(entity);

            if(transform && transform->IsDirty() && HasComponent<ModelComponent>(entity))
            {
                BoundingBox*    worldBB = GetComponent<BoundingBox>(entity);
                ModelComponent* modelComp = GetComponent<ModelComponent>(entity);

                if(!worldBB)
                {
                    worldBB = AddComponent<BoundingBox>(entity);
                    if(!worldBB) { continue; }
                }

                auto modelAsset = ResourceManager::GetModel(modelComp->modelHandle);

                if(modelAsset)
                {
                    *worldBB = BoundingBox::LocalToGlobal(modelAsset->GetLocalBoundingBox(), transform->Mat4());
                    transform->SetDirty(false);
                }
                else { worldBB->valid = false; }

                worldBB->valid = true;
            }

            else if(transform && HasComponent<BoundingBox>(entity) && !HasComponent<ModelComponent>(entity))
            {
                BoundingBox* worldBB = GetComponent<BoundingBox>(entity);
                if(worldBB) { worldBB->valid = false; }
            }
        }
    }

    template <typename T> SparseSet<T>& GetComponentStorage();

private:
    // Const version for HasComponent
    // template <typename T> const SparseSet<T>& GetComponentStorage() const;

    SparseSet<TransformComponent>   m_transforms{};
    SparseSet<ModelComponent>       m_models{};
    SparseSet<BoundingBox>          m_worldBoundingBoxes{};
    SparseSet<AudioSourceComponent> m_audioSources{};
    id_t                            m_nextEntity = 0;
};

} // namespace Humongous
