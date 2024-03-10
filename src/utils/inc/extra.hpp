#pragma once

#include <functional>
#include <model.hpp>
#include <string>
#include <vector>
#include

namespace Humongous
{
namespace Utils
{
std::vector<char> ReadFile(const std::string& filePath);

template <typename T, typename... Rest> void HashCombine(std::size_t& seed, const T& v, const Rest&... rest)
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (HashCombine(seed, rest), ...);
};

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
