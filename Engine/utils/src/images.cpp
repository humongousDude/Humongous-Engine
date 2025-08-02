#include "images.hpp"
#include "logger.hpp"

namespace Humongous
{
namespace Utils
{
void CreateAllocatedImage(const ILogicalDevice& logicalDevice, u32 width, u32 height, vk::Format format, vk::ImageTiling tiling,
                          vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, AllocatedImage& allocatedImage,
                          vk::ImageAspectFlags aspectFlags)
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

    if(logicalDevice.GetAllocator().AllocateImage(imageInfo, allocatedImage.allocation, allocatedImage.image) != vk::Result::eSuccess)
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

    if(logicalDevice.CreateImageView(viewInfo, &allocatedImage.imageView) != vk::Result::eSuccess) { HGERROR("Failed to create image view"); }

    allocatedImage.width = width;
    allocatedImage.height = height;
    allocatedImage.mipLevels = 1;
    allocatedImage.layerCount = 1;
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
    imageInfo.initialLayout = createInfo.initialLayout;
    imageInfo.usage = createInfo.usage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.samples = createInfo.samples;
    imageInfo.flags = createInfo.flags;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(createInfo.properties);
    allocInfo.pool = (createInfo.imagePool == VK_NULL_HANDLE) ? nullptr : createInfo.imagePool;

    if(createInfo.logicalDevice.GetAllocator().AllocateImage(imageInfo, createInfo.allocatedImage->allocation, createInfo.allocatedImage->image) !=
       vk::Result::eSuccess)
    {
        HGERROR("Failed to create image");
    }

    createInfo.logicalDevice.GetAllocator().NameAllocation(createInfo.allocatedImage->allocation, createInfo.name.c_str());

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = createInfo.allocatedImage->image;
    viewInfo.viewType = createInfo.imageViewType;
    viewInfo.format = createInfo.format;
    viewInfo.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
    viewInfo.subresourceRange = {createInfo.aspectFlags, 0, createInfo.mipLevels, 0, createInfo.layerCount};
    if(createInfo.aspectFlags == (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil))
    {
        viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    }

    if(createInfo.logicalDevice.CreateImageView(viewInfo, &createInfo.allocatedImage->imageView) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create image view");
    }

    createInfo.allocatedImage->width = createInfo.width;
    createInfo.allocatedImage->height = createInfo.height;
    createInfo.allocatedImage->mipLevels = createInfo.mipLevels;
    createInfo.allocatedImage->layerCount = createInfo.layerCount;

    if(!createInfo.createWithSampler) { return; }

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = createInfo.samplerInfo->magFilter;
    samplerInfo.minFilter = createInfo.samplerInfo->minFilter;
    samplerInfo.addressModeU = createInfo.samplerInfo->addressModeU;
    samplerInfo.addressModeV = createInfo.samplerInfo->addressModeV;
    samplerInfo.addressModeW = createInfo.samplerInfo->addressModeW;
    samplerInfo.anisotropyEnable = createInfo.logicalDevice.GetPhysicalDevice().GetFeatures().features.samplerAnisotropy;
    samplerInfo.maxAnisotropy =
        samplerInfo.anisotropyEnable ? createInfo.logicalDevice.GetPhysicalDevice().GetProperties().properties.limits.maxSamplerAnisotropy : 1.0f;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueWhite;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = vk::CompareOp::eNever;
    samplerInfo.mipmapMode = createInfo.samplerInfo->mipMode;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(createInfo.mipLevels);
    samplerInfo.pNext = createInfo.samplerInfo->pNext;

    // CHECKME: is this a correct way to create a sampler?
    createInfo.allocatedImage->sampler = new vk::Sampler;
    *createInfo.allocatedImage->sampler = createInfo.logicalDevice.CreateSampler(samplerInfo);
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
    if(info.newLayout == info.oldLayout)
    {
        HGWARN("Identical layouts, skipping transition");
        return;
    }
    if(info.layerCount < 1)
    {
        HGWARN("Layer count is less than 1, skipping transition");
        return;
    }

    vk::ImageMemoryBarrier2 imageBarrier{};
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.oldLayout = info.oldLayout;
    imageBarrier.newLayout = info.newLayout;
    imageBarrier.image = info.image;
    imageBarrier.subresourceRange.aspectMask = info.imageAspect;
    imageBarrier.subresourceRange.baseMipLevel = info.baseMipLevel;
    imageBarrier.subresourceRange.levelCount = info.levelCount;
    imageBarrier.subresourceRange.baseArrayLayer = info.baseArrayLayer;
    imageBarrier.subresourceRange.layerCount = info.layerCount;

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

void CopyImageToImage(const ILogicalDevice& logicalDevice, vk::CommandBuffer cmd, vk::Image src, vk::Image dst, vk::Extent2D srcSize,
                      vk::Extent2D dstSize)
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

    logicalDevice.RecordBlitImage(cmd, blitInfo);
}

void CopyBufferToImage(const ILogicalDevice& logicalDevice, vk::Buffer buffer, vk::Image image, u32 width, u32 height)
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

    logicalDevice.RecordCopyBufferToImage(commandBuffer, buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
    logicalDevice.EndSingleTimeCommands(commandBuffer);
}

void CopyBufferToImage(const ILogicalDevice& logicalDevice, vk::Buffer buffer, vk::Image image, const std::vector<vk::BufferImageCopy>& regions)
{
    if(buffer == VK_NULL_HANDLE || image == VK_NULL_HANDLE)
    {
        HGERROR("Unable to copy buffer to image, buffer or image is null");
        return;
    }

    vk::CommandBuffer commandBuffer = logicalDevice.BeginSingleTimeCommands();
    logicalDevice.RecordCopyBufferToImage(commandBuffer, buffer, image, vk::ImageLayout::eTransferDstOptimal, regions);
    logicalDevice.EndSingleTimeCommands(commandBuffer);
}

} // namespace Utils
} // namespace Humongous
