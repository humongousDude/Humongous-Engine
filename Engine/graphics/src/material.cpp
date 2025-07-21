#include "material.hpp"

namespace Humongous
{

BoundingBox BoundingBox::LocalToGlobal(const BoundingBox& localBoundingBox, const Eigen::Matrix4f& model)
{
    if(!localBoundingBox.valid) { return BoundingBox{}; }

    Eigen::Vector3f worldMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Eigen::Vector3f worldMax(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

    std::array<Eigen::Vector3f, 8> localCorners = {localBoundingBox.min.head<3>(),
                                                   Eigen::Vector3f(localBoundingBox.max.x(), localBoundingBox.min.y(), localBoundingBox.min.z()),
                                                   Eigen::Vector3f(localBoundingBox.min.x(), localBoundingBox.max.y(), localBoundingBox.min.z()),
                                                   Eigen::Vector3f(localBoundingBox.max.x(), localBoundingBox.max.y(), localBoundingBox.min.z()),
                                                   Eigen::Vector3f(localBoundingBox.min.x(), localBoundingBox.min.y(), localBoundingBox.max.z()),
                                                   Eigen::Vector3f(localBoundingBox.max.x(), localBoundingBox.min.y(), localBoundingBox.max.z()),
                                                   Eigen::Vector3f(localBoundingBox.min.x(), localBoundingBox.max.y(), localBoundingBox.max.z()),
                                                   localBoundingBox.max.head<3>()};

    BoundingBox worldBoundingBox{};

    for(int i = 0; i < 8; ++i)
    {
        Eigen::Vector4f localCornerHomogeneous(localCorners[i].x(), localCorners[i].y(), localCorners[i].z(), 1.0f);

        Eigen::Vector4f transformedCornerHomogeneous = model * localCornerHomogeneous;

        Eigen::Vector3f transformedCorner;
        // if(transformedCornerHomogeneous.w() != 0.0f)
        // {
        //     transformedCorner = transformedCornerHomogeneous.head<3>() / transformedCornerHomogeneous.w();
        // }
        // else {  }
        transformedCorner = transformedCornerHomogeneous.head<3>();
        worldMin = worldMin.cwiseMin(transformedCorner);
        worldMax = worldMax.cwiseMax(transformedCorner);
    }

    worldBoundingBox.min = Eigen::Vector4f(worldMin.x(), worldMin.y(), worldMin.z(), 1.0f);
    worldBoundingBox.max = Eigen::Vector4f(worldMax.x(), worldMax.y(), worldMax.z(), 1.0f);
    worldBoundingBox.valid = true;

    return worldBoundingBox;
}
} // namespace Humongous
