#include <logger.hpp>
#include <material.hpp>
#include <model.hpp>

namespace Humongous
{

BoundingBox BoundingBox::LocalToGlobal(const BoundingBox& localBoundingBox, const glm::mat4& model)
{
    BoundingBox worldBoundingBox{};

    std::array<glm::vec4, 8> localCornersHomogeneous;
    localCornersHomogeneous[0] = glm::vec4(localBoundingBox.min.x, localBoundingBox.min.y, localBoundingBox.min.z, 1.0f);
    localCornersHomogeneous[1] = glm::vec4(localBoundingBox.max.x, localBoundingBox.min.y, localBoundingBox.min.z, 1.0f);
    localCornersHomogeneous[2] = glm::vec4(localBoundingBox.min.x, localBoundingBox.max.y, localBoundingBox.min.z, 1.0f);
    localCornersHomogeneous[3] = glm::vec4(localBoundingBox.max.x, localBoundingBox.max.y, localBoundingBox.min.z, 1.0f);
    localCornersHomogeneous[4] = glm::vec4(localBoundingBox.min.x, localBoundingBox.min.y, localBoundingBox.max.z, 1.0f);
    localCornersHomogeneous[5] = glm::vec4(localBoundingBox.max.x, localBoundingBox.min.y, localBoundingBox.max.z, 1.0f);
    localCornersHomogeneous[6] = glm::vec4(localBoundingBox.min.x, localBoundingBox.max.y, localBoundingBox.max.z, 1.0f);
    localCornersHomogeneous[7] = glm::vec4(localBoundingBox.max.x, localBoundingBox.max.y, localBoundingBox.max.z, 1.0f);

    std::array<glm::vec4, 8> worldCorners;
    for(int i = 0; i < 8; ++i)
    {
        glm::vec4 worldCornerHomogeneous = model * localCornersHomogeneous[i];
        worldCorners[i] = glm::vec4(worldCornerHomogeneous); // Convert back from homogeneous coordinates (discard w)
    }

    worldBoundingBox.min = glm::vec4(std::numeric_limits<float>::max());
    worldBoundingBox.max = glm::vec4(std::numeric_limits<float>::lowest());

    for(const auto& corner: worldCorners)
    {
        worldBoundingBox.min = glm::min(worldBoundingBox.min, corner);
        worldBoundingBox.max = glm::max(worldBoundingBox.max, corner);
    }

    worldBoundingBox.valid = true;

    // for(int i = 0; i < 8; ++i) { worldBoundingBox.corners[i] = worldCorners[i]; }

    return worldBoundingBox;
}
} // namespace Humongous
