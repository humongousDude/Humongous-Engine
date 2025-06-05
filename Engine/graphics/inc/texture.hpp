#pragma once

#include "images.hpp"
#include "logical_device.hpp"
#include <gli.hpp>
#include <string>

namespace tinygltf
{
class Image;
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

    Texture(LogicalDevice* m_logicalDevice, const std::string& imagePath, const ImageType& imageType = ImageType::TEX2D,
            const bool& storage = false);
    Texture() : m_logicalDevice{nullptr} {};

    void FillWithEmpty(LogicalDevice* m_logicalDevice, n32 width, n32 height, const bool& storage = false);

    vk::DescriptorImageInfo GetDescriptorInfo() const { return {m_textureSampler, m_textureImage.imageView, m_textureImage.imageLayout}; };

    vk::Image       GetRawImageHandle() const { return m_textureImage.image; }
    vk::ImageView   GetRawImageViewHandle() const { return m_textureImage.imageView; }
    vk::ImageLayout GetRawImageLayout() const { return m_textureImage.imageLayout; }
    vk::Sampler     GetRawSamplerHandle() const { return m_textureSampler; }

    AllocatedImage& GetAllocatedImage() { return m_textureImage; }

    n32 GetWidth() const { return m_width; }
    n32 GetHeight() const { return m_height; }
    n32 GetMipLevels() const { return m_miplevels; }
    n32 GetLayerCount() const { return m_layerCount; }
    n32 GetBaseSize() const { return m_baseSize; }

    void Destroy();

    void CreateFromGLTFImage(tinygltf::Image& gltfimage, TexSamplerInfo textureSampler, LogicalDevice* device, vk::Queue copyQueue);
    void CreateFromFile(const std::string& path, LogicalDevice* device, const ImageType& imageType = ImageType::TEX2D, const bool& storage = false);

private:
    LogicalDevice* m_logicalDevice;
    AllocatedImage m_textureImage;
    vk::Sampler    m_textureSampler;

    n32 m_width, m_height, m_miplevels, m_layerCount, m_baseSize;

    void CreateTextureImage(const std::string& imagePath, const ImageType& imageType = ImageType::TEX2D, const bool& storage = false);
    void CreateTextureImageSampler(const TexSamplerInfo& samplerInfo, const ImageType& imageType = ImageType::TEX2D);
};
}; // namespace Humongous
