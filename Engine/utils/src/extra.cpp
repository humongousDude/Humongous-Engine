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

vk::ShaderModule CreateShaderModule(const ILogicalDevice& logicalDevice, const std::string& shaderFile)
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

void TransitionImageLayout(ImageTransitionInfo& info)
{
    if(info.cmd == VK_NULL_HANDLE)
    {
        HGERROR("Unable to transition image layout, command buffer is null");
        return;
    }
    if(info.image == VK_NULL_HANDLE)
    {
        HGERROR("Unable to transition image layout, image is null");
        return;
    }

    b8 needsQueueTransfer = (info.srcQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED && info.dstQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED &&
                             info.srcQueueFamilyIndex != info.dstQueueFamilyIndex);
    if(info.newLayout == info.oldLayout && !needsQueueTransfer)
    {
        HGWARN("Identical layouts and no queue transfer, skipping transition");
        return;
    }

    if(info.layerCount < 1)
    {
        HGWARN("Layer count is less than 1, skipping transition");
        return;
    }

    vk::ImageMemoryBarrier2 imageBarrier{};
    imageBarrier.oldLayout = info.oldLayout;
    imageBarrier.newLayout = info.newLayout;
    imageBarrier.image = info.image;
    imageBarrier.subresourceRange.aspectMask = info.imageAspect;
    imageBarrier.subresourceRange.baseMipLevel = info.baseMipLevel;
    imageBarrier.subresourceRange.levelCount = info.levelCount;
    imageBarrier.subresourceRange.baseArrayLayer = info.baseArrayLayer;
    imageBarrier.subresourceRange.layerCount = info.layerCount;

    imageBarrier.srcQueueFamilyIndex = needsQueueTransfer ? info.srcQueueFamilyIndex : VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = needsQueueTransfer ? info.dstQueueFamilyIndex : VK_QUEUE_FAMILY_IGNORED;

    switch(info.oldLayout)
    {
        case vk::ImageLayout::eUndefined:
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eNone;
            break;

        case vk::ImageLayout::eColorAttachmentOptimal:
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
            break;

        case vk::ImageLayout::eDepthAttachmentOptimal:
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eLateFragmentTests;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
            break;

        case vk::ImageLayout::eTransferSrcOptimal:
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead;
            break;

        case vk::ImageLayout::eTransferDstOptimal:
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
            break;

        case vk::ImageLayout::eShaderReadOnlyOptimal:
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eShaderRead;
            break;

        case vk::ImageLayout::eGeneral:
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
            break;

        default:
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
            break;
    }

    switch(info.newLayout)
    {
        case vk::ImageLayout::eColorAttachmentOptimal:
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
            break;

        case vk::ImageLayout::eDepthAttachmentOptimal:
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
            break;

        case vk::ImageLayout::eTransferSrcOptimal:
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
            break;

        case vk::ImageLayout::eTransferDstOptimal:
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
            break;

        case vk::ImageLayout::eShaderReadOnlyOptimal:
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
            break;

        case vk::ImageLayout::eGeneral:
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite;
            break;

        case vk::ImageLayout::ePresentSrcKHR:
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eNone;
            break;

        default:
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;
            break;
    }

    vk::DependencyInfo depInfo{};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    info.logicalDevice.RecordPipelineBarrier(info.cmd, depInfo);
}

void TransitionImageLayout(const ILogicalDevice& logicalDevice, vk::Image image, vk::ImageLayout currentLayout, vk::ImageLayout newLayout)
{
    vk::CommandBuffer   cmd = logicalDevice.BeginSingleTimeCommands();
    ImageTransitionInfo info{.logicalDevice = logicalDevice};
    info.cmd = cmd;
    info.oldLayout = currentLayout;
    info.newLayout = newLayout;
    info.image = image;

    TransitionImageLayout(info);

    logicalDevice.EndSingleTimeCommands(cmd);
}

} // namespace Humongous::Utils
