#include "abstractions/buffer.hpp"
#include "asserts.hpp"
#include "defines.hpp"
#include "images.hpp"
#include "logger.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <gli/texture.hpp>
#include <gli/texture_cube.hpp>
#include <texture.hpp>
#include <tiny_gltf.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Humongous
{
Texture::Texture(LogicalDevice* logicalDevice, const std::string& imagePath, const ImageType& imageType) : m_logicalDevice{logicalDevice}
{
    CreateFromFile(imagePath, logicalDevice, imageType);
}

void Texture::Destroy()
{
    vkDestroySampler(m_logicalDevice->GetVkDevice(), m_textureSampler, nullptr);
    vkDestroyImageView(m_logicalDevice->GetVkDevice(), m_textureImage.imageView, nullptr);
    vmaDestroyImage(m_logicalDevice->GetVmaAllocator(), m_textureImage.image, m_textureImage.allocation);
}

void Texture::CreateFromFile(const std::string& path, LogicalDevice* device, const ImageType& imageType)
{
    this->m_logicalDevice = device;

    CreateTextureImage(path, imageType);

    descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorInfo.imageView = m_textureImage.imageView;
    descriptorInfo.sampler = m_textureSampler;
}

void Texture::CreateFromGLTFImage(tinygltf::Image& gltfimage, TexSamplerInfo textureSampler, LogicalDevice* device, VkQueue copyQueue)
{
    m_logicalDevice = device;

    unsigned char* buffer = nullptr;
    VkDeviceSize   bufferSize = 0;
    bool           deleteBuffer = false;
    if(gltfimage.component == 3)
    {
        // Most devices don't support RGB only on Vulkan so convert if necessary
        // TODO: Check actual format support and transform only if required
        bufferSize = gltfimage.width * gltfimage.height * 4;
        buffer = new unsigned char[bufferSize];
        unsigned char* rgba = buffer;
        unsigned char* rgb = &gltfimage.image[0];
        for(int32_t i = 0; i < gltfimage.width * gltfimage.height; ++i)
        {
            for(int32_t j = 0; j < 3; ++j) { rgba[j] = rgb[j]; }
            rgba += 4;
            rgb += 3;
        }
        deleteBuffer = true;
    }
    else
    {
        buffer = &gltfimage.image[0];
        bufferSize = gltfimage.image.size();
    }

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    VkFormatProperties formatProperties;

    width = gltfimage.width;
    height = gltfimage.height;
    mipLevels = static_cast<uint32_t>(floor(log2(std::max(width, height))) + 1.0);

    vkGetPhysicalDeviceFormatProperties(m_logicalDevice->GetPhysicalDevice().GetVkPhysicalDevice(), format, &formatProperties);
    HGASSERT(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT);
    HGASSERT(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);

    Buffer stagingBuffer{m_logicalDevice,
                         bufferSize,
                         1,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU};
    stagingBuffer.Map();

    stagingBuffer.WriteToBuffer((void*)buffer);

    Utils::AllocatedImageCreateInfo createInfo{.logicalDevice = *m_logicalDevice, .allocatedImage = m_textureImage};
    createInfo.width = width;
    createInfo.height = height;
    createInfo.mipLevels = mipLevels;
    createInfo.layerCount = 1;
    createInfo.format = format;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    createInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.flags = 0;
    createInfo.imageViewType = VK_IMAGE_VIEW_TYPE_2D;

    Utils::CreateAllocatedImage(createInfo);

    Utils::TransitionImageLayout(*m_logicalDevice, m_textureImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Utils::CopyBufferToImage(*m_logicalDevice, stagingBuffer.GetBuffer(), m_textureImage.image, static_cast<u32>(width), static_cast<u32>(height));
    Utils::TransitionImageLayout(*m_logicalDevice, m_textureImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = textureSampler.magFilter;
    samplerInfo.minFilter = textureSampler.minFilter;
    samplerInfo.addressModeU = textureSampler.addressModeU;
    samplerInfo.addressModeV = textureSampler.addressModeV;
    samplerInfo.addressModeW = textureSampler.addressModeW;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 1.0;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.maxAnisotropy = 1.0;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxLod = (float)mipLevels;
    samplerInfo.maxAnisotropy = 8.0f;
    samplerInfo.anisotropyEnable = VK_TRUE;
    if(vkCreateSampler(m_logicalDevice->GetVkDevice(), &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS)
    {
        HGERROR("Failed to create texture sampler");
    }

    descriptorInfo.imageView = m_textureImage.imageView;
    descriptorInfo.sampler = m_textureSampler;
    descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if(deleteBuffer) { delete[] buffer; }
}

void Texture::CreateTextureImage(const std::string& imagePath, const ImageType& imageType)
{
    SamplerCreateInfo samplerInfo{};

    if(imageType == ImageType::TEX2D)
    {
        int      texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(imagePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        width = texWidth;
        height = texHeight;
        mipLevels = 1;

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

        Utils::AllocatedImageCreateInfo createInfo{.logicalDevice = *m_logicalDevice, .allocatedImage = m_textureImage};
        createInfo.width = width;
        createInfo.height = height;
        createInfo.mipLevels = mipLevels;
        createInfo.layerCount = 1;
        createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        createInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        createInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

        Utils::CreateAllocatedImage(createInfo);

        Utils::TransitionImageLayout(*m_logicalDevice, m_textureImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        Utils::CopyBufferToImage(*m_logicalDevice, stagingBuffer.GetBuffer(), m_textureImage.image, static_cast<u32>(width),
                                 static_cast<u32>(height));
        Utils::TransitionImageLayout(*m_logicalDevice, m_textureImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        CreateTextureImageSampler(samplerInfo);
    }
    else if(imageType == ImageType::CUBEMAP)
    {
        gli::texture_cube texCube(gli::load(imagePath));
        HGASSERT(!texCube.empty() && "Failed to load texture!");
        width = static_cast<uint32_t>(texCube.extent().x);
        height = static_cast<uint32_t>(texCube.extent().y);
        mipLevels = static_cast<uint32_t>(texCube.levels());

        Buffer stagingBuffer{m_logicalDevice,
                             texCube.size(),
                             1,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU};
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)texCube.data());

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        size_t                         offset = 0;

        for(u32 face = 0; face < 6; face++)
        {
            for(u32 level = 0; level < mipLevels; level++)
            {
                VkBufferImageCopy bufferCopyRegion = {};
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = level;
                bufferCopyRegion.imageSubresource.baseArrayLayer = face;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(texCube[face][level].extent().x);
                bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(texCube[face][level].extent().y);
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;

                bufferCopyRegions.push_back(bufferCopyRegion);

                // Increase offset into staging buffer for next level / face
                offset += texCube[face][level].size();
            }
        }

        Utils::AllocatedImageCreateInfo createInfo{.logicalDevice = *m_logicalDevice, .allocatedImage = m_textureImage};
        createInfo.layerCount = 6;
        createInfo.mipLevels = mipLevels;
        createInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        createInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        createInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.width = static_cast<u32>(width);
        createInfo.height = static_cast<u32>(height);
        createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        createInfo.imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;

        Utils::CreateAllocatedImage(createInfo);

        Utils::TransitionImageLayout(*m_logicalDevice, m_textureImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        Utils::CopyBufferToImage(*m_logicalDevice, stagingBuffer.GetBuffer(), m_textureImage.image, bufferCopyRegions);

        Utils::TransitionImageLayout(*m_logicalDevice, m_textureImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        CreateTextureImageSampler(samplerInfo, ImageType::CUBEMAP);
    }
}

void Texture::CreateTextureImageSampler(const SamplerCreateInfo& info, const ImageType& imageType)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = info.magFilter;
    samplerInfo.minFilter = info.minFilter;
    samplerInfo.addressModeU = info.addressModeU;
    samplerInfo.addressModeV = info.addressModeV;
    samplerInfo.addressModeW = info.addressModeW;
    samplerInfo.anisotropyEnable = m_logicalDevice->GetPhysicalDevice().GetFeatures().features.samplerAnisotropy;
    // for now this is hardcoded
    // planning to make LogicalDevice and PhysicalDevice Singletons
    // to make access easier, as the engine will only use one GPU
    samplerInfo.maxAnisotropy =
        samplerInfo.anisotropyEnable ? m_logicalDevice->GetPhysicalDevice().GetProperties().properties.limits.maxSamplerAnisotropy : 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevels);
    if(vkCreateSampler(m_logicalDevice->GetVkDevice(), &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS)
    {
        HGERROR("Failed to create texture sampler");
    }
}

}; // namespace Humongous
