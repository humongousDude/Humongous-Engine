#include "images.hpp"
#include "logger.hpp"
#include <texture.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Humongous
{
Texture::Texture(LogicalDevice& logicalDevice, const std::string& imagePath) : m_logicalDevice{logicalDevice}
{
    CreateTextureImage(imagePath);
    CreateTextureImageSampler();
}

Texture::~Texture()
{
    vkDestroySampler(m_logicalDevice.GetVkDevice(), m_textureSampler, nullptr);
    vkDestroyImageView(m_logicalDevice.GetVkDevice(), m_textureImage.imageView, nullptr);
    vmaDestroyImage(m_logicalDevice.GetVmaAllocator(), m_textureImage.image, m_textureImage.allocation);
}

void Texture::CreateTextureImage(const std::string& imagePath)
{
    int      texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(imagePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if(!pixels) { HGERROR("Failed to load texture image at path: %s", "textures/texture.jpg"); }

    Buffer stagingBuffer{m_logicalDevice,
                         imageSize,
                         1,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU};
    stagingBuffer.Map();
    stagingBuffer.WriteToBuffer((void*)pixels);

    stbi_image_free(pixels);

    Utils::CreateAllocatedImage(m_logicalDevice, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_textureImage,
                                VK_IMAGE_ASPECT_COLOR_BIT);

    Utils::TransitionImageLayout(m_logicalDevice, m_textureImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Utils::CopyBufferToImage(m_logicalDevice, stagingBuffer.GetBuffer(), m_textureImage.image, static_cast<u32>(texWidth),
                             static_cast<u32>(texHeight));
    Utils::TransitionImageLayout(m_logicalDevice, m_textureImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Texture::CreateTextureImageSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    // for now this is hardcoded
    // planning to make LogicalDevice and PhysicalDevice Singletons
    // to make access easier, as the engine will only use one GPU
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    if(vkCreateSampler(m_logicalDevice.GetVkDevice(), &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS)
    {
        HGERROR("Failed to create texture sampler");
    }
}

}; // namespace Humongous
