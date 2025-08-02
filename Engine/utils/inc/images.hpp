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
    vk::Sampler*    sampler{nullptr};
    u32             width = 1, height = 1, mipLevels = 1, layerCount = 1;

    vk::DescriptorImageInfo GetDescriptorInfo() const
    {
        if(sampler) { return {*sampler, imageView, imageLayout}; }
        else { return {VK_NULL_HANDLE, imageView, imageLayout}; }
    };

    void Destroy(const ILogicalDevice& logicalDevice)
    {
        logicalDevice.DestroyImageView(imageView);
        logicalDevice.GetAllocator().FreeImage(allocation, image);
        if(sampler) { logicalDevice.DestroySampler(*sampler); }
    }
};

namespace Utils
{

struct SamplerCreateInfo
{
    vk::Filter             magFilter;
    vk::Filter             minFilter;
    vk::SamplerAddressMode addressModeU;
    vk::SamplerAddressMode addressModeV;
    vk::SamplerAddressMode addressModeW;
    vk::SamplerMipmapMode  mipMode{vk::SamplerMipmapMode::eLinear};
    const void*            pNext{nullptr};
};

struct AllocatedImageCreateInfo
{
    const ILogicalDevice&   logicalDevice;
    u32                     width, height, mipLevels, layerCount;
    vk::Format              format;
    vk::ImageTiling         tiling;
    vk::ImageUsageFlags     usage;
    vk::MemoryPropertyFlags properties;
    AllocatedImage*         allocatedImage;
    b32                     createWithSampler{false};
    SamplerCreateInfo*      samplerInfo = nullptr;
    vk::ImageLayout         initialLayout = vk::ImageLayout::eUndefined;
    vk::ImageAspectFlags    aspectFlags = vk::ImageAspectFlagBits::eColor;
    vk::ImageCreateFlags    flags{};
    vk::ImageViewType       imageViewType = vk::ImageViewType::e2D;
    VmaPool                 imagePool{VK_NULL_HANDLE};
    vk::SampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
    std::string             name = "you should name me!";
};

struct ImageTransitionInfo
{
    vk::CommandBuffer     cmd;
    vk::ImageLayout       oldLayout = vk::ImageLayout::eUndefined;
    vk::ImageLayout       newLayout = vk::ImageLayout::eUndefined;
    const ILogicalDevice& logicalDevice;
    vk::Image             image;
    vk::ImageAspectFlags  imageAspect = vk::ImageAspectFlagBits::eColor;
    u32                   baseMipLevel = 0;
    u32                   levelCount = 1;
    u32                   baseArrayLayer = 0;
    u32                   layerCount = 1;
};

void CreateAllocatedImage(const ILogicalDevice& logicalDevice, u32 width, u32 height, vk::Format format, vk::ImageTiling tiling,
                          vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, AllocatedImage& allocatedImage,
                          vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor);

void CreateAllocatedImage(AllocatedImageCreateInfo& createInfo);

/**
 *  Quick helper function, usage not recommended
 */
void TransitionImageLayout(const ILogicalDevice& logicalDevice, vk::Image image, vk::ImageLayout currentLayout, vk::ImageLayout newLayout);

void TransitionImageLayout(ImageTransitionInfo& info);

void CopyImageToImage(const ILogicalDevice& logicalDevice, vk::CommandBuffer cmd, vk::Image src, vk::Image dst, vk::Extent2D srcSize,
                      vk::Extent2D dstSize);

void CopyBufferToImage(const ILogicalDevice& logicalDevice, vk::Buffer buffer, vk::Image image, u32 width, u32 height);
void CopyBufferToImage(const ILogicalDevice& logicalDevice, vk::Buffer buffer, vk::Image image,
                       const std::vector<vk::BufferImageCopy>& bufferCopyRegions);

} // namespace Utils
} // namespace Humongous
