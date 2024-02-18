#pragma once

#include "logical_device.hpp"
#include <abstractions/buffer.hpp>
#include <gli/gli.hpp>
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

    Texture(LogicalDevice* m_logicalDevice, const std::string& imagePath, const ImageType& imageType = ImageType::TEX2D);
    Texture() : m_logicalDevice{nullptr} {};

    VkDescriptorImageInfo GetDescriptorInfo() const { return descriptorInfo; };

    void Destroy();

    void CreateFromGLTFImage(tinygltf::Image& gltfimage, TexSamplerInfo textureSampler, LogicalDevice* device, VkQueue copyQueue);
    void CreateFromFile(const std::string& path, LogicalDevice* device, const ImageType& imageType = ImageType::TEX2D);

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

    VkDescriptorImageInfo descriptorInfo;

    u32 width, height, mipLevels, layerCount;

    void CreateTextureImage(const std::string& imagePath, const ImageType& imageType = ImageType::TEX2D);
    void CreateTextureImageSampler(const SamplerCreateInfo& samplerInfo, const ImageType& imageType = ImageType::TEX2D);
};
}; // namespace Humongous
