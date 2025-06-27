#include "extra.hpp"
#include "globals.hpp"
#include "logger.hpp"

#include <algorithm>
#include <fstream>

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

std::vector<VisibleEntityInfo> SortAndCullEntities(Camera& camera, World& world)
{
    std::vector<VisibleEntityInfo> visibleEntities;

    for(const auto& entityId: world.GetComponentStorage<ModelComponent>().GetDense())
    {
        BoundingBox* bb = world.GetComponent<BoundingBox>(entityId);
        if(!bb || !bb->valid) // Bounding box must exist and be valid
        {
            continue;
        }

        ModelComponent* model = world.GetComponent<ModelComponent>(entityId);
        if(!model) { continue; }

        TransformComponent* transform = world.GetComponent<TransformComponent>(entityId);

        if(!camera.IsAABBInsideFrustum(bb->min, bb->max)) { continue; }
        if(glm::distance(transform->GetTranslation(), camera.GetPosition()) > static_cast<n32>(Globals::Limits::MaximumRenderDistance))
        {
            continue;
        }

        float distance = glm::distance(camera.GetPosition(), transform->GetTranslation());

        visibleEntities.push_back({entityId, distance});
    }

    // 4. Sort visible entities (closest to farthest, as in your original code)
    std::ranges::sort(visibleEntities,
                      [](const VisibleEntityInfo& a, const VisibleEntityInfo& b) { return (a.distanceToCamera < b.distanceToCamera); });

    return visibleEntities;
}

} // namespace Humongous::Utils
