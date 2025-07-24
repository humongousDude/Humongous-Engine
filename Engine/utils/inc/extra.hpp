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

namespace Humongous
{
namespace Utils
{

std::vector<char> ReadFile(const std::string& filePath);

struct VisibleEntityInfo
{
    Humongous::EntityID id;
    f32                 distanceToCamera;
};

std::vector<VisibleEntityInfo> SortAndCullEntities(Camera& camera, World& world);

template <typename T, typename... Rest> void HashCombine(std::size_t& seed, const T& v, const Rest&... rest)
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (HashCombine(seed, rest), ...);
};

void DecomposeMatrix(const Eigen::Matrix4f& matrix, Eigen::Vector3f& translation, Eigen::Quaternionf& rotation, Eigen::Vector3f& scale);

inline f32 DegreesToRadians(f32 degrees) { return degrees * (M_PI / 180.0f); }

vk::ShaderModule CreateShaderModule(const LogicalDevice& logicalDevice, const std::string& shaderFile);

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

template <> struct hash<Eigen::Vector3f>
{
    size_t operator()(const Eigen::Vector3f& v) const
    {
        // A simple component-wise hash.
        // For better distribution, you might want a more sophisticated
        // hash combining function, but this is a good start.
        size_t    seed = 0;
        hash<f32> hasher;
        seed ^= hasher(v.x()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(v.y()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(v.z()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
template <> struct hash<Eigen::Vector4f>
{
    size_t operator()(const Eigen::Vector4f& v) const
    {
        // Combining the hashes of individual components is a common approach.
        // The magic number 0x9e3779b9 is often used in hash combining.
        size_t    seed = 0;
        hash<f32> hasher; // Hash for f32 components

        seed ^= hasher(v.x()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(v.y()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(v.z()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(v.w()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
template <> struct hash<Eigen::Matrix4f>
{
    size_t operator()(const Eigen::Matrix4f& m) const
    {
        size_t    seed = 0;
        hash<f32> hasher;
        for(int i = 0; i < m.rows(); ++i)
        {
            for(int j = 0; j < m.cols(); ++j) { seed ^= hasher(m(i, j)) + 0x9e3779b9 + (seed << 6) + (seed >> 2); }
        }
        return seed;
    }
};

template <> struct hash<Eigen::Matrix<float, 2, 1, 0, 2, 1>>
{
    size_t operator()(const Eigen::Matrix<float, 2, 1, 0, 2, 1>& m) const
    {
        size_t    seed = 0;
        hash<f32> hasher;
        for(int i = 0; i < m.rows(); ++i)
        {
            for(int j = 0; j < m.cols(); ++j) { seed ^= hasher(m(i, j)) + 0x9e3779b9 + (seed << 6) + (seed >> 2); }
        }
        return seed;
    }
};

} // namespace std
