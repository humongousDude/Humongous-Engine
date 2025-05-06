#pragma once

#include "defines.hpp"
#include "logical_device.hpp"

namespace Humongous
{

struct AllocatedImage
{
    vk::Image       image{VK_NULL_HANDLE};
    vk::ImageView   imageView{VK_NULL_HANDLE};
    VmaAllocation   allocation;
    vk::Extent3D    imageExtent;
    vk::Format      imageFormat;
    vk::ImageLayout imageLayout;
};

namespace Utils
{

struct AllocatedImageCreateInfo
{
    LogicalDevice&          logicalDevice;
    n32                     width, height, mipLevels, layerCount;
    vk::Format              format;
    vk::ImageTiling         tiling;
    vk::ImageUsageFlags     usage;
    vk::MemoryPropertyFlags properties;
    AllocatedImage&         allocatedImage;
    vk::ImageAspectFlags    aspectFlags = vk::ImageAspectFlagBits::eColor;
    vk::ImageCreateFlags    flags{};
    vk::ImageViewType       imageViewType = vk::ImageViewType::e2D;
    VmaPool                 imagePool{VK_NULL_HANDLE};
    vk::SampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
};

struct ImageTransitionInfo
{
    vk::CommandBuffer    cmd;
    vk::ImageLayout      oldLayout;
    vk::ImageLayout      newLayout;
    LogicalDevice*       logicalDevice;
    vk::Image            image;
    vk::ImageAspectFlags imageAspect = vk::ImageAspectFlagBits::eNone;
    n32                  baseMipLevel = 0;
    n32                  levelCount = 1;
    n32                  baseArrayLayer = 0;
    n32                  layerCount = 1;
};

void CreateAllocatedImage(LogicalDevice& logicalDevice, n32 width, n32 height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                          vk::MemoryPropertyFlags properties, AllocatedImage& allocatedImage,
                          vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor);

void CreateAllocatedImage(AllocatedImageCreateInfo& createInfo);

/**
 *  Quick helper function, usage not recommended
 */
void TransitionImageLayout(LogicalDevice& logicalDevice, vk::Image image, vk::ImageLayout currentLayout, vk::ImageLayout newLayout);

void TransitionImageLayout(ImageTransitionInfo& info);

void CopyImageToImage(vk::CommandBuffer cmd, vk::Image src, vk::Image dst, vk::Extent2D srcSize, vk::Extent2D dstSize);
void CopyImageToImage(vk::CommandBuffer cmd, AllocatedImage& src, AllocatedImage& dst, std::vector<vk::ImageBlit>& blits);

void CopyBufferToImage(LogicalDevice& logicalDevice, vk::Buffer buffer, vk::Image image, n32 width, n32 height);
void CopyBufferToImage(LogicalDevice& logicalDevice, vk::Buffer buffer, vk::Image image, const std::vector<vk::BufferImageCopy>& bufferCopyRegions);

} // namespace Utils
} // namespace Humongous
