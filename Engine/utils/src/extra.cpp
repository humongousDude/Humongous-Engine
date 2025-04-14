#include "gameobject.hpp"
#include <algorithm>
#include <extra.hpp>
#include <fstream>
#include <logger.hpp>


namespace Humongous::Utils
{

std::vector<char> ReadFile(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if(!file.is_open()) { HGERROR("Failed to open file: %s", filePath.c_str()); }

    const size_t fileSize = file.tellg();

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

std::vector<std::pair<GameObject::id_t, GameObject*>> SortAndCullGameObjects(Camera& camera, GameObject::Map& unsortedObjects)
{
    std::vector<std::pair<GameObject::id_t, GameObject*>> sortedObjects;
    sortedObjects.clear();
    sortedObjects.reserve(unsortedObjects.size());

    // Store key-pointer pairs
    for(auto& [key, gameObject]: unsortedObjects)
    {
        if(!gameObject.model) { continue; }
        if(!camera.IsAABBInsideFrustum(gameObject.GetBoundingBox().min, gameObject.GetBoundingBox().max)) { continue; }
        sortedObjects.emplace_back(key, &gameObject);
    }

    // Sort based on distance
    std::ranges::sort(sortedObjects, [&camera](const auto& a, const auto& b) {
        const float distA = glm::distance(glm::vec3(camera.GetPosition()), a.second->transform.translation);
        const float distB = glm::distance(glm::vec3(camera.GetPosition()), b.second->transform.translation);
        return distA < distB;
    });
    return sortedObjects;
}

} // namespace Humongous::Utils

