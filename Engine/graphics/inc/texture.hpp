#pragma once

#include "logical_device.hpp"
#include <abstractions/buffer.hpp>
#include <gli.hpp>
#include <renderer.hpp>
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
        VkFilter             magFilter;
        VkFilter             minFilter;
        VkSamplerAddressMode addressModeU;
        VkSamplerAddressMode addressModeV;
        VkSamplerAddressMode addressModeW;
    };

    Texture(LogicalDevice* m_logicalDevice, const std::string& imagePath, const ImageType& imageType = ImageType::TEX2D,
            const bool& storage = false);
    Texture() : m_logicalDevice{nullptr} {};

    void FillWithEmpty(LogicalDevice* m_logicalDevice, n32 width, n32 height, const bool& storage = false);

    VkDescriptorImageInfo GetDescriptorInfo() const { return {m_textureSampler, m_textureImage.imageView, m_textureImage.imageLayout}; };

    VkImage       GetRawImageHandle() const { return m_textureImage.image; }
    VkImageView   GetRawImageViewHandle() const { return m_textureImage.imageView; }
    VkImageLayout GetRawImageLayout() const { return m_textureImage.imageLayout; }
    VkSampler     GetRawSamplerHandle() const { return m_textureSampler; }

    AllocatedImage& GetAllocatedImage() { return m_textureImage; }

    n32 GetWidth() const { return m_width; }
    n32 GetHeight() const { return m_height; }
    n32 GetMipLevels() const { return m_miplevels; }
    n32 GetLayerCount() const { return m_layerCount; }
    n32 GetBaseSize() const { return m_baseSize; }

    void Destroy();

    void CreateFromGLTFImage(tinygltf::Image& gltfimage, TexSamplerInfo textureSampler, LogicalDevice* device, VkQueue copyQueue);
    void CreateFromFile(const std::string& path, LogicalDevice* device, const ImageType& imageType = ImageType::TEX2D, const bool& storage = false);

private:
    struct SamplerCreateInfo
    {
        VkFilter             magFilter;
        VkFilter             minFilter;
        VkSamplerAddressMode addressModeU;
        VkSamplerAddressMode addressModeV;
        VkSamplerAddressMode addressModeW;
    };

    LogicalDevice* m_logicalDevice;
    AllocatedImage m_textureImage;
    VkSampler      m_textureSampler;

    n32 m_width, m_height, m_miplevels, m_layerCount, m_baseSize;

    void CreateTextureImage(const std::string& imagePath, const ImageType& imageType = ImageType::TEX2D, const bool& storage = false);
    void CreateTextureImageSampler(const SamplerCreateInfo& samplerInfo, const ImageType& imageType = ImageType::TEX2D);
};
}; // namespace Humongous
