#pragma once

#include "camera.hpp"
#include "model.hpp"
#include "world.hpp"

#include <functional>
#include <string>
#include <vector>

#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/hash.hpp"

namespace Humongous
{
namespace Utils
{

std::vector<char> ReadFile(const std::string& filePath);

// old way, entities were called gameobjects
// Structure to hold information about entities that are visible
struct VisibleEntityInfo
{
    Humongous::EntityID id;               // The ID of the entity
    float               distanceToCamera; // For sorting
    // You could add pointers to components here if you need them immediately after sorting
    // and want to avoid another lookup, but usually, the ID is sufficient.
};

std::vector<VisibleEntityInfo> SortAndCullEntities(Camera& camera, World& world);

template <typename T, typename... Rest> void HashCombine(std::size_t& seed, const T& v, const Rest&... rest)
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (HashCombine(seed, rest), ...);
};

void DecomposeMatrix(const glm::mat4& matrix, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale);

} // namespace Utils
} // namespace Humongous

namespace std
{
template <> struct hash<Humongous::Model::Vertex>
{
    size_t operator()(Humongous::Model::Vertex const& vertex) const
    {
        size_t seed = 0;
        Humongous::Utils::HashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv0, vertex.uv1);
        return seed;
    }
};

} // namespace std
