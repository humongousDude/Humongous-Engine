#pragma once

#include "abstractions/image.hpp"
#include "defines.hpp"
#include "logical_device.hpp"
#include <string>

namespace tinygltf
{
struct Image;
};

namespace Humongous
{
class Texture
{
public:
    enum class ImageType
    {
        TEX2D,
        CUBEMAP,
    };

    struct TexSamplerInfo
    {
        vk::Filter             magFilter;
        vk::Filter             minFilter;
        vk::SamplerAddressMode addressModeU;
        vk::SamplerAddressMode addressModeV;
        vk::SamplerAddressMode addressModeW;
    };

    Texture(const ILogicalDevice& logicalDevice, const std::string& imagePath, const ImageType& imageType = ImageType::TEX2D,
            const b8& storage = false);
    Texture(const ILogicalDevice& logicalDevice) : m_logicalDevice{logicalDevice} {};

    void FillWithEmpty(const ILogicalDevice& m_logicalDevice, u32 width, u32 height, const b8& storage = false);

    vk::DescriptorImageInfo GetDescriptorInfo() const
    {
        if(!m_textureImage) { return {}; }
        else
        {
            return {m_textureSampler, m_textureImage->GetImageView(), m_textureImage->GetLayout()};
        }
    };

    vk::Image       GetRawImageHandle() const { return m_textureImage->GetImage(); }
    vk::ImageView   GetRawImageViewHandle() const { return m_textureImage->GetImageView(); }
    vk::ImageLayout GetRawImageLayout() const { return m_textureImage->GetLayout(); }
    vk::Sampler     GetRawSamplerHandle() const { return m_textureSampler; }

    Image& GetAllocatedImage() { return *m_textureImage; }

    u32 GetWidth() const { return m_width; }
    u32 GetHeight() const { return m_height; }
    u32 GetMipLevels() const { return m_miplevels; }
    u32 GetLayerCount() const { return m_layerCount; }
    u32 GetBaseSize() const { return m_baseSize; }

    void Destroy();

    void CreateFromGLTFImage(tinygltf::Image& gltfimage, TexSamplerInfo textureSampler);
    void CreateFromFile(const std::string& path, const ImageType& imageType = ImageType::TEX2D, const b8& storage = false);

private:
    const ILogicalDevice&  m_logicalDevice;
    std::unique_ptr<Image> m_textureImage{nullptr};
    vk::Sampler            m_textureSampler;

    u32 m_width, m_height, m_miplevels, m_layerCount, m_baseSize;

    void CreateTextureImage(const std::string& imagePath, const ImageType& imageType = ImageType::TEX2D, const b8& storage = false);
    void CreateTextureImageSampler(const TexSamplerInfo& samplerInfo);
    void GenerateMipmaps(vk::CommandBuffer commandBuffer, vk::Image image, u32 texWidth, u32 texHeight, u32 mipLevels, vk::ImageLayout finalLayout);
};
}; // namespace Humongous
