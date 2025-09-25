#include "abstractions/image.hpp"
#include "logger.hpp"

namespace Humongous
{

Image::Image(const ILogicalDevice& device, const u32& width, const u32& height, vk::ImageLayout layout)
    : m_logicalDevice(device), m_width(width), m_height(height), m_currentQueue(0)
{
    ImageCreateInfo createInfo{};
    createInfo.width = width;
    createInfo.height = height;
    createInfo.queue = 0;
    createInfo.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
    m_usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
    createInfo.format = vk::Format::eR8G8B8A8Unorm;
    m_format = vk::Format::eR8G8B8A8Unorm;
    AllocateImage(createInfo);
}

Image::Image(const ILogicalDevice& device, const ImageCreateInfo& createInfo)
    : m_logicalDevice(device), m_width(createInfo.width), m_height(createInfo.height), m_mipLevels(createInfo.mipLevels),
      m_layerCount(createInfo.layerCount), m_arrayLayerCount(createInfo.arrayLayerCount), m_aspectFlags(createInfo.aspectFlags),
      m_currentQueue(createInfo.queue), m_usage(createInfo.usage)
{
    AllocateImage(createInfo);
}
Image::~Image() { Destroy(m_logicalDevice); }

void Image::Destroy(const ILogicalDevice& logicalDevice)
{
    if(m_imageView) { logicalDevice.DestroyImageView(m_imageView); }
    if(m_image) { logicalDevice.GetAllocator().FreeImage(m_allocation, m_image); }
}

void Image::AllocateImage(const ImageCreateInfo& createInfo)
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
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.flags = createInfo.flags;
    imageInfo.sharingMode = createInfo.sharingMode;

    if(createInfo.sharingMode != vk::SharingMode::eExclusive)
    {
        // imageInfo.queueFamilyIndexCount = static_cast<u32>(createInfo.queueFamilyIndices.size());
        // imageInfo.pQueueFamilyIndices = createInfo.queueFamilyIndices.data();
    }

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(createInfo.properties);

    if(m_logicalDevice.GetAllocator().AllocateImage(imageInfo, allocInfo, m_allocation, m_image) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create image");
        return;
    }

    m_logicalDevice.GetAllocator().NameAllocation(m_allocation, createInfo.name.c_str());

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = m_image;
    viewInfo.viewType = createInfo.imageViewType;
    viewInfo.format = createInfo.format;
    viewInfo.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
    viewInfo.subresourceRange = {createInfo.aspectFlags, 0, createInfo.mipLevels, 0, createInfo.layerCount};
    if(createInfo.aspectFlags == (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil))
    {
        viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    }

    if(m_logicalDevice.CreateImageView(viewInfo, &m_imageView) != vk::Result::eSuccess) { HGERROR("Failed to create image view"); }

    if(!createInfo.createWithSampler) { return; }

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = createInfo.samplerInfo->magFilter;
    samplerInfo.minFilter = createInfo.samplerInfo->minFilter;
    samplerInfo.addressModeU = createInfo.samplerInfo->addressModeU;
    samplerInfo.addressModeV = createInfo.samplerInfo->addressModeV;
    samplerInfo.addressModeW = createInfo.samplerInfo->addressModeW;
    samplerInfo.anisotropyEnable = m_logicalDevice.GetPhysicalDevice().GetFeatures().features.samplerAnisotropy;
    samplerInfo.maxAnisotropy =
        samplerInfo.anisotropyEnable ? m_logicalDevice.GetPhysicalDevice().GetProperties().properties.limits.maxSamplerAnisotropy : 1.0f;
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
    m_sampler = new vk::Sampler;
    *m_sampler = m_logicalDevice.CreateSampler(samplerInfo);

    m_layout = vk::ImageLayout::eUndefined;

    std::string key = createInfo.name + "_" + std::to_string(m_width) + "x" + std::to_string(m_height) + "_ch" + vk::to_string(m_format);

    m_hashId = key;
}

void Image::TransitionLayout(vk::CommandBuffer cmd, vk::ImageLayout newLayout, u32 baseMip, u32 mipCount, u32 baseArrayLayer, u32 arrayLayerCount)
{
    HGTRACE("Trying to transition image layout from %s to %s", vk::to_string(m_layout).c_str(), vk::to_string(newLayout).c_str());
    if(cmd == VK_NULL_HANDLE)
    {
        HGERROR("Unable to transition image layout, command buffer is null");
        return;
    }

    if(m_image == VK_NULL_HANDLE)
    {
        HGERROR("Unable to transition image layout, image is null");
        return;
    }

    if(arrayLayerCount < 1)
    {
        HGWARN("Layer count is less than 1, skipping transition");
        return;
    }

    if(newLayout == m_layout)
    {
        HGTRACE("Identical layouts, skipping transition");
        return;
    }

    vk::ImageMemoryBarrier2 imageBarrier{};
    imageBarrier.oldLayout = m_layout;
    imageBarrier.newLayout = newLayout;
    imageBarrier.image = m_image;
    imageBarrier.subresourceRange.aspectMask = m_aspectFlags;
    imageBarrier.subresourceRange.baseMipLevel = baseMip;
    imageBarrier.subresourceRange.levelCount = mipCount;
    imageBarrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    imageBarrier.subresourceRange.layerCount = arrayLayerCount;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    switch(m_layout)
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
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eAllCommands;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
            break;

        default:
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
            break;
    }

    switch(newLayout)
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

    m_logicalDevice.RecordPipelineBarrier(cmd, depInfo);

    m_layout = newLayout;
    HGTRACE("Transitioned image layout to %s", vk::to_string(newLayout).c_str());
}

void Image::TransitionQueue(vk::CommandBuffer cmd, u32 newQueue)
{
    if(m_currentQueue == newQueue)
    {
        HGWARN("Queue ownership is the same, skipping transfer");
        return;
    }

    if(cmd == VK_NULL_HANDLE)
    {
        HGERROR("Unable to transfer image ownership, command buffer is null");
        return;
    }

    if(m_image == VK_NULL_HANDLE)
    {
        HGERROR("Unable to transfer image ownership, image is null");
        return;
    }

    vk::ImageMemoryBarrier2 imageBarrier{};
    imageBarrier.oldLayout = m_layout;
    imageBarrier.newLayout = m_layout;
    imageBarrier.image = m_image;
    imageBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = m_mipLevels;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = m_layerCount;
    imageBarrier.srcQueueFamilyIndex = m_currentQueue;
    imageBarrier.dstQueueFamilyIndex = newQueue;

    // TODO: Accurate access and stage masks
    imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    imageBarrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
    imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    imageBarrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;

    vk::DependencyInfo depInfo{};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    m_logicalDevice.RecordPipelineBarrier(cmd, depInfo);

    m_currentQueue = newQueue;
}

void Image::CopyToImage(vk::CommandBuffer cmd, const Image& dst, vk::Extent2D srcSize, vk::Extent2D dstSize)
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
    blitInfo.srcImage = m_image;
    blitInfo.srcImageLayout = m_layout;
    blitInfo.dstImage = dst.GetImage();
    blitInfo.dstImageLayout = dst.GetLayout();
    blitInfo.filter = vk::Filter::eLinear;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    m_logicalDevice.RecordBlitImage(cmd, blitInfo);
}

void Image::CopyToImage(const ILogicalDevice& dev, vk::CommandBuffer cmd, vk::Image src, vk::Image dst, vk::Extent2D srcSize, vk::Extent2D dstSize)
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

    dev.RecordBlitImage(cmd, blitInfo);
}

} // namespace Humongous
