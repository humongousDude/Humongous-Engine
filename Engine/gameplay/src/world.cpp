#include "world.hpp"

namespace Humongous
{

template <> SparseSet<TransformComponent>&   World::GetComponentStorage<TransformComponent>() { return m_transforms; }
template <> SparseSet<ModelComponent>&       World::GetComponentStorage<ModelComponent>() { return m_models; }
template <> SparseSet<BoundingBox>&          World::GetComponentStorage<BoundingBox>() { return m_worldBoundingBoxes; }
template <> SparseSet<AudioSourceComponent>& World::GetComponentStorage<AudioSourceComponent>() { return m_audioSources; }

} // namespace Humongous
