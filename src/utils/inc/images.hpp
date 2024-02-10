#pragma once

#include "defines.hpp"
#include <logical_device.hpp>

namespace Humongous
{

struct AllocatedImage
{
    VkImage       image{VK_NULL_HANDLE};
    VkImageView   imageView{VK_NULL_HANDLE};
    VmaAllocation allocation;
    VkExtent3D    imageExtent;
    VkFormat      imageFormat;
};

namespace Utils
{

void CreateAllocatedImage(LogicalDevice& logicalDevice, u32 width, u32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                          VkMemoryPropertyFlags properties, AllocatedImage& allocatedImage,
                          VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

void TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

void TransitionImageLayout(LogicalDevice& logicalDevice, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

void CopyImageToImage(VkCommandBuffer cmd, VkImage src, VkImage dst, VkExtent2D srcSize, VkExtent2D dstSize);

void CopyBufferToImage(LogicalDevice& logicalDevice, VkBuffer buffer, VkImage image, u32 width, u32 height);

} // namespace Utils
} // namespace Humongous
