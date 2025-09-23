#pragma once
#include "defines.hpp"
#include "logger.hpp"
#include "logical_device.hpp"
#include "non_copyable.hpp"

namespace Humongous
{

class Image : NonCopyable
{
public:
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
    struct ImageCreateInfo
    {
        u32                     width{1}, height{1}, mipLevels{1}, layerCount{1}, arrayLayerCount{1};
        vk::Format              format{vk::Format::eR8G8B8A8Unorm};
        vk::ImageTiling         tiling{vk::ImageTiling::eLinear};
        vk::ImageUsageFlags     usage{};
        vk::MemoryPropertyFlags properties{};
        b32                     createWithSampler{false};
        SamplerCreateInfo*      samplerInfo = nullptr;
        vk::ImageAspectFlags    aspectFlags = vk::ImageAspectFlagBits::eColor;
        vk::ImageCreateFlags    flags{};
        vk::ImageViewType       imageViewType = vk::ImageViewType::e2D;
        std::string             name = "you should name me!";
        vk::SharingMode         sharingMode = vk::SharingMode::eExclusive;
        u32                     queue = 0;
    };

    // Create an empty unallocated image
    Image(const ILogicalDevice& device) : m_logicalDevice(device) {}

    // convenience constructor for creating an empty image with a specific layout on the graphics queue
    Image(const ILogicalDevice& device, const u32& width, const u32& height, vk::ImageLayout layout);

    // Create an image with details specified in the createInfo struct
    Image(const ILogicalDevice& device, const ImageCreateInfo& createInfo);
    ~Image();

    void TransitionLayout(vk::CommandBuffer cmd, vk::ImageLayout newLayout, u32 baseMip = 0, u32 mipCount = vk::RemainingMipLevels,
                          u32 baseArrayLayer = 0, u32 arrayLayerCount = vk::RemainingArrayLayers);
    void TransitionQueue(vk::CommandBuffer cmd, u32 queue);

    vk::ImageLayout GetLayout() const { return m_layout; }
    u32             GetWidth() const { return m_width; }
    u32             GetHeight() const { return m_height; }
    u32             GetMipLevels() const { return m_mipLevels; }
    u32             GetLayerCount() const { return m_layerCount; }
    u32             GetArrayLayerCount() const { return m_arrayLayerCount; }

    const vk::Image&     GetImage() const { return m_image; }
    const vk::ImageView& GetImageView() const { return m_imageView; }

    std::string GetID() const { return m_hashId; }

    void CopyToImage(vk::CommandBuffer cmd, const Image& dst, vk::Extent2D srcSize, vk::Extent2D dstSize);
    // static void CopyToImage(vk::CommandBuffer cmd, const Image& src, const Image& dst, vk::Extent2D srcSize, vk::Extent2D dstSize);
    static void CopyToImage(const ILogicalDevice& dev, vk::CommandBuffer cmd, vk::Image src, vk::Image dst, vk::Extent2D srcSize,
                            vk::Extent2D dstSize);

    b8                      IsValid() const { return m_image != VK_NULL_HANDLE; }
    vk::DescriptorImageInfo GetDescriptorInfo() const
    {
        if(m_imageView == VK_NULL_HANDLE)
        {
            HGERROR("Attempting to get descriptor info of an image without an image view!");
            return {};
        }
        if(m_sampler) { return {*m_sampler, m_imageView, m_layout}; }
        else
        {
            return {VK_NULL_HANDLE, m_imageView, m_layout};
        }
    };

    void Destroy(const ILogicalDevice& logicalDevice);

private:
    const ILogicalDevice& m_logicalDevice;
    u32                   m_width, m_height, m_mipLevels, m_layerCount, m_arrayLayerCount, m_currentQueue;

    vk::Image            m_image{VK_NULL_HANDLE};
    vk::ImageView        m_imageView{VK_NULL_HANDLE};
    VmaAllocation        m_allocation;
    vk::Extent3D         m_extent{vk::Extent3D{0, 0, 0}};
    vk::Format           m_format{vk::Format::eUndefined};
    vk::ImageLayout      m_layout{vk::ImageLayout::eUndefined};
    vk::Sampler*         m_sampler{nullptr};
    vk::ImageAspectFlags m_aspectFlags{};
    vk::ImageUsageFlags  m_usage{};

    std::string m_hashId;

    void AllocateImage(const ImageCreateInfo& createInfo);
};

} // namespace Humongous
