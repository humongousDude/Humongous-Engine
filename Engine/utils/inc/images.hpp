#pragma once

#include "defines.hpp"
#include "logical_device.hpp"

namespace Humongous
{

struct AllocatedImage
{
    VkImage       image{VK_NULL_HANDLE};
    VkImageView   imageView{VK_NULL_HANDLE};
    VmaAllocation allocation;
    VkExtent3D    imageExtent;
    VkFormat      imageFormat;
    VkImageLayout imageLayout;
};

namespace Utils
{

struct AllocatedImageCreateInfo
{
    LogicalDevice&        logicalDevice;
    u32                   width, height, mipLevels, layerCount;
    VkFormat              format;
    VkImageTiling         tiling;
    VkImageUsageFlags     usage;
    VkMemoryPropertyFlags properties;
    AllocatedImage&       allocatedImage;
    VkImageAspectFlags    aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageCreateFlags    flags = 0;
    VkImageViewType       imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    VmaPool               imagePool{VK_NULL_HANDLE};
    VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
};

struct ImageTransitionInfo
{
    VkCommandBuffer cmd;
    VkImageLayout   oldLayout;
    VkImageLayout   newLayout;
    LogicalDevice*  logicalDevice;
    VkImage         image;
    u32             baseMipLevel = 0;
    u32             levelCount = 1;
    u32             baseArrayLayer = 0;
    u32             layerCount = 1;
};

void CreateAllocatedImage(LogicalDevice& logicalDevice, u32 width, u32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                          VkMemoryPropertyFlags properties, AllocatedImage& allocatedImage,
                          VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

void CreateAllocatedImage(AllocatedImageCreateInfo& createInfo);

void TransitionImageLayout(LogicalDevice& logicalDevice, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
void TransitionImageLayout(ImageTransitionInfo& info);

void CopyImageToImage(VkCommandBuffer cmd, VkImage src, VkImage dst, VkExtent2D srcSize, VkExtent2D dstSize);
void CopyImageToImage(VkCommandBuffer cmd, AllocatedImage& src, AllocatedImage& dst, std::vector<VkImageBlit>& blits);

void CopyBufferToImage(LogicalDevice& logicalDevice, VkBuffer buffer, VkImage image, u32 width, u32 height);
void CopyBufferToImage(LogicalDevice& logicalDevice, VkBuffer buffer, VkImage image, const std::vector<VkBufferImageCopy>& bufferCopyRegions);

} // namespace Utils
} // namespace Humongous
