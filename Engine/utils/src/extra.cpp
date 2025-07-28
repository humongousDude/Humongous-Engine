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
    if(!file.is_open())
    {
        HGERROR("Failed to open file: %s", filePath.c_str());
        return std::vector<char>();
    }

    HGINFO("Reading file: %s", filePath.c_str());

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
        if(!bb || !bb->valid) { continue; }

        ModelComponent* model = world.GetComponent<ModelComponent>(entityId);
        if(!model) { continue; }

        TransformComponent* transform = world.GetComponent<TransformComponent>(entityId);
        if(!transform) { continue; }

        if(!camera.IsAABBInsideFrustum(bb->min.head<3>(), bb->max.head<3>())) { continue; }

        float distance_sq = (transform->GetTranslation() - camera.GetPosition()).squaredNorm();
        float distance = std::sqrt(distance_sq);

        if(distance > static_cast<f32>(Globals::Limits::MaximumRenderDistance)) { continue; }

        visibleEntities.push_back({entityId, distance});
    }

    std::ranges::sort(visibleEntities,
                      [](const VisibleEntityInfo& a, const VisibleEntityInfo& b) { return (a.distanceToCamera < b.distanceToCamera); });

    return visibleEntities;
}

void DecomposeMatrix(const Eigen::Matrix4f& matrix, Eigen::Vector3f& translation, Eigen::Quaternionf& rotation, Eigen::Vector3f& scale)
{
    translation = matrix.col(3).head<3>();

    Eigen::Matrix3f rotationScaleMatrix = matrix.block<3, 3>(0, 0);

    scale.x() = rotationScaleMatrix.col(0).norm();
    scale.y() = rotationScaleMatrix.col(1).norm();
    scale.z() = rotationScaleMatrix.col(2).norm();

    if(scale.x() != 0.0f) { rotationScaleMatrix.col(0) /= scale.x(); }
    if(scale.y() != 0.0f) { rotationScaleMatrix.col(1) /= scale.y(); }
    if(scale.z() != 0.0f) { rotationScaleMatrix.col(2) /= scale.z(); }

    rotation = Eigen::Quaternionf(rotationScaleMatrix);
}

vk::ShaderModule CreateShaderModule(const LogicalDevice& logicalDevice, const std::string& shaderFile)
{
    std::vector<char> shaderCode = ReadFile(shaderFile);
    if(shaderCode.empty()) { return VK_NULL_HANDLE; }

    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const u32*>(shaderCode.data());

    vk::ShaderModule shaderModule;
    if(logicalDevice.GetVkDevice().createShaderModule(&createInfo, nullptr, &shaderModule) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create shader module!");
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

} // namespace Humongous::Utils
