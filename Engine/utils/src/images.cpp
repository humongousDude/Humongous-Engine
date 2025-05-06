#pragma once

#include "images.hpp"
#include "logger.hpp"

namespace Humongous
{
namespace Utils
{
void CreateAllocatedImage(LogicalDevice& logicalDevice, n32 width, n32 height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                          vk::MemoryPropertyFlags properties, AllocatedImage& allocatedImage, vk::ImageAspectFlags aspectFlags)
{
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = usage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.samples = vk::SampleCountFlagBits::e1;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(properties);

    if(vmaCreateImage(logicalDevice.GetVmaAllocator(), reinterpret_cast<const VkImageCreateInfo*>(&imageInfo), &allocInfo,
                      reinterpret_cast<VkImage*>(&allocatedImage.image), &allocatedImage.allocation, nullptr) != VK_SUCCESS)
    {
        HGERROR("Failed to create image");
    }

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = allocatedImage.image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if(logicalDevice.GetVkDevice().createImageView(&viewInfo, nullptr, &allocatedImage.imageView) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create image view");
    }
}

void CreateAllocatedImage(AllocatedImageCreateInfo& createInfo)
{
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent.width = createInfo.width;
    imageInfo.extent.height = createInfo.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = createInfo.mipLevels;
    imageInfo.arrayLayers = createInfo.layerCount;
    imageInfo.format = createInfo.format;
    imageInfo.tiling = createInfo.tiling;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = createInfo.usage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.samples = createInfo.samples;
    imageInfo.flags = createInfo.flags;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(createInfo.properties);
    allocInfo.pool = (createInfo.imagePool == VK_NULL_HANDLE) ? nullptr : createInfo.imagePool;

    if(vmaCreateImage(createInfo.logicalDevice.GetVmaAllocator(), reinterpret_cast<const VkImageCreateInfo*>(&imageInfo), &allocInfo,
                      reinterpret_cast<VkImage*>(&createInfo.allocatedImage.image), &createInfo.allocatedImage.allocation, nullptr) != VK_SUCCESS)
    {
        HGERROR("Failed to create image");
    }

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = createInfo.allocatedImage.image;
    viewInfo.viewType = createInfo.imageViewType;
    viewInfo.format = createInfo.format;
    viewInfo.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
    viewInfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, createInfo.mipLevels, 0, createInfo.layerCount};
    viewInfo.subresourceRange.aspectMask = createInfo.aspectFlags;

    if(createInfo.logicalDevice.GetVkDevice().createImageView(&viewInfo, nullptr, &createInfo.allocatedImage.imageView) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create image view");
    }
}

void TransitionImageLayout(ImageTransitionInfo& info)
{
    auto& logicalDevice = *info.logicalDevice;
    auto  currentLayout = info.oldLayout;
    auto  newLayout = info.newLayout;

    vk::ImageMemoryBarrier2 imageBarrier{};
    imageBarrier.srcStageMask = vk::PipelineStageFlags2{};
    imageBarrier.dstStageMask = vk::PipelineStageFlags2{};

    if(currentLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        imageBarrier.srcAccessMask = {};
        imageBarrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;

        imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;
        imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    }
    else if(currentLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        imageBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        imageBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;

        imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
    }
    else
    {
        imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
        imageBarrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
        imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
        imageBarrier.dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;
    }

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    vk::ImageAspectFlags aspectMask =
        info.imageAspect
            ? info.imageAspect
            : ((newLayout == vk::ImageLayout::eDepthAttachmentOptimal) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor);

    imageBarrier.subresourceRange.aspectMask = aspectMask;
    imageBarrier.subresourceRange.baseMipLevel = info.baseMipLevel;
    imageBarrier.subresourceRange.levelCount = info.levelCount;
    imageBarrier.subresourceRange.baseArrayLayer = info.baseArrayLayer;
    imageBarrier.subresourceRange.layerCount = info.layerCount;
    imageBarrier.image = info.image;

    vk::DependencyInfo depInfo{};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    info.cmd.pipelineBarrier2(depInfo);
}

void TransitionImageLayout(LogicalDevice& logicalDevice, vk::Image image, vk::ImageLayout currentLayout, vk::ImageLayout newLayout)
{
    vk::CommandBuffer   cmd = logicalDevice.BeginSingleTimeCommands();
    ImageTransitionInfo info{};
    info.cmd = cmd;
    info.oldLayout = currentLayout;
    info.newLayout = newLayout;
    info.logicalDevice = &logicalDevice;
    info.image = image;

    TransitionImageLayout(info);

    logicalDevice.EndSingleTimeCommands(cmd);
}

void CopyImageToImage(vk::CommandBuffer cmd, vk::Image src, vk::Image dst, vk::Extent2D srcSize, vk::Extent2D dstSize)
{
    vk::ImageBlit2 blitRegion{};
    blitRegion.srcOffsets[1] = vk::Offset3D(srcSize.width, srcSize.height, 1);
    blitRegion.dstOffsets[1] = vk::Offset3D(dstSize.width, dstSize.height, 1);

    blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = 0;

    blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstSubresource.mipLevel = 0;

    vk::BlitImageInfo2 blitInfo{};
    blitInfo.srcImage = src;
    blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
    blitInfo.dstImage = dst;
    blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
    blitInfo.filter = vk::Filter::eLinear;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    cmd.blitImage2(blitInfo);
}

void CopyBufferToImage(LogicalDevice& logicalDevice, vk::Buffer buffer, vk::Image image, n32 width, n32 height)
{
    vk::CommandBuffer   commandBuffer = logicalDevice.BeginSingleTimeCommands();
    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D(0, 0, 0);
    region.imageExtent = vk::Extent3D(width, height, 1);

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);
    logicalDevice.EndSingleTimeCommands(commandBuffer);
}

void CopyBufferToImage(LogicalDevice& logicalDevice, vk::Buffer buffer, vk::Image image, const std::vector<vk::BufferImageCopy>& regions)
{
    vk::CommandBuffer commandBuffer = logicalDevice.BeginSingleTimeCommands();
    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, static_cast<uint32_t>(regions.size()), regions.data());
    logicalDevice.EndSingleTimeCommands(commandBuffer);
}

} // namespace Utils
} // namespace Humongous
