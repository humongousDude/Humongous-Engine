#pragma once

#include "camera.hpp"
#include "model.hpp"
#include "world.hpp"

#include <Eigen/Dense>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace std
{
template <> struct hash<Eigen::Vector3f>
{
    size_t operator()(const Eigen::Vector3f& v) const
    {
        // A simple component-wise hash.
        // For better distribution, you might want a more sophisticated
        // hash combining function, but this is a good start.
        size_t      seed = 0;
        hash<float> hasher;
        seed ^= hasher(v.x()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(v.y()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(v.z()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
} // namespace std

// Custom hash for Eigen::Vector4f
namespace std
{
template <> struct hash<Eigen::Vector4f>
{
    size_t operator()(const Eigen::Vector4f& v) const
    {
        // Combining the hashes of individual components is a common approach.
        // The magic number 0x9e3779b9 is often used in hash combining.
        size_t      seed = 0;
        hash<float> hasher; // Hash for float components

        seed ^= hasher(v.x()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(v.y()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(v.z()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(v.w()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
} // namespace std

namespace std
{
template <> struct hash<Eigen::Matrix4f>
{
    size_t operator()(const Eigen::Matrix4f& m) const
    {
        size_t      seed = 0;
        hash<float> hasher;
        for(int i = 0; i < m.rows(); ++i)
        {
            for(int j = 0; j < m.cols(); ++j) { seed ^= hasher(m(i, j)) + 0x9e3779b9 + (seed << 6) + (seed >> 2); }
        }
        return seed;
    }
};
} // namespace std

namespace Humongous
{
namespace Utils
{

std::vector<char> ReadFile(const std::string& filePath);

struct VisibleEntityInfo
{
    Humongous::EntityID id;
    float               distanceToCamera;
};

std::vector<VisibleEntityInfo> SortAndCullEntities(Camera& camera, World& world);

template <typename T, typename... Rest> void HashCombine(std::size_t& seed, const T& v, const Rest&... rest)
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (HashCombine(seed, rest), ...);
};

void DecomposeMatrix(const Eigen::Matrix4f& matrix, Eigen::Vector3f& translation, Eigen::Quaternionf& rotation, Eigen::Vector3f& scale);

inline float DegreesToRadians(float degrees) { return degrees * (M_PI / 180.0f); }

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
