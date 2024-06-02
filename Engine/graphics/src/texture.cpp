#include "abstractions/buffer.hpp"
#include "allocator.hpp"
#include "asserts.hpp"
#include "defines.hpp"
#include "images.hpp"
#include "logger.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <gli/texture.hpp>
#include <gli/texture_cube.hpp>
#include <texture.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <tiny_gltf.h>

namespace Humongous
{
Texture::Texture(LogicalDevice* logicalDevice, const std::string& imagePath, const ImageType& imageType) : m_logicalDevice{logicalDevice}
{
    CreateFromFile(imagePath, logicalDevice, imageType);
}

void Texture::Destroy()
{
    if(m_textureSampler) { vkDestroySampler(m_logicalDevice->GetVkDevice(), m_textureSampler, nullptr); }

    if(m_textureImage.imageView != VK_NULL_HANDLE) { vkDestroyImageView(m_logicalDevice->GetVkDevice(), m_textureImage.imageView, nullptr); }
    if(m_textureImage.image != VK_NULL_HANDLE)
    {
        vmaDestroyImage(m_logicalDevice->GetVmaAllocator(), m_textureImage.image, m_textureImage.allocation);
    }
}

void Texture::CreateFromFile(const std::string& path, LogicalDevice* device, const ImageType& imageType)
{
    this->m_logicalDevice = device;

    CreateTextureImage(path, imageType);
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

    VkFormat           format = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormatProperties formatProperties;

    width = gltfimage.width;
    height = gltfimage.height;
    mipLevels = static_cast<u32>(floor(log2(std::max(width, height))) + 1.0);

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
    stagingBuffer.WriteToBuffer((void*)buffer, bufferSize);

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
    createInfo.imagePool = Allocator::GetglTFImagePool();

    Utils::CreateAllocatedImage(createInfo);

    Utils::TransitionImageLayout(*m_logicalDevice, m_textureImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_textureImage.imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkBufferImageCopy bufferCopyRegion{};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = width;
    bufferCopyRegion.imageExtent.height = height;
    bufferCopyRegion.imageExtent.depth = 1;

    std::vector<VkBufferImageCopy> bufferCopyRegions{bufferCopyRegion};

    Utils::CopyBufferToImage(*m_logicalDevice, stagingBuffer.GetBuffer(), m_textureImage.image, bufferCopyRegions);

    Utils::TransitionImageLayout(*m_logicalDevice, m_textureImage.image, m_textureImage.imageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    m_textureImage.imageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    VkCommandBuffer blitCmd = m_logicalDevice->BeginSingleTimeCommands();

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;

    for(u32 i = 1; i < mipLevels; i++)
    {
        VkImageBlit2 imageBlit{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2};

        imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlit.srcSubresource.layerCount = 1;
        imageBlit.srcSubresource.mipLevel = i - 1;
        imageBlit.srcOffsets[1].x = i32(width >> (i - 1));
        imageBlit.srcOffsets[1].y = i32(height >> (i - 1));
        imageBlit.srcOffsets[1].z = 1;

        imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlit.dstSubresource.layerCount = 1;
        imageBlit.dstSubresource.mipLevel = i;
        imageBlit.dstOffsets[1].x = i32(width >> i);
        imageBlit.dstOffsets[1].y = i32(height >> i);
        imageBlit.dstOffsets[1].z = 1;

        VkImageSubresourceRange mipSubRange = {};
        mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        mipSubRange.baseMipLevel = i;
        mipSubRange.levelCount = 1;
        mipSubRange.layerCount = 1;

        {
            Utils::ImageTransitionInfo imageTransition{};
            imageTransition.image = m_textureImage.image;
            imageTransition.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageTransition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageTransition.cmd = blitCmd;
            imageTransition.logicalDevice = m_logicalDevice;
            imageTransition.layerCount = mipSubRange.layerCount;
            imageTransition.levelCount = mipSubRange.levelCount;
            imageTransition.baseMipLevel = mipSubRange.baseMipLevel;
            imageTransition.baseArrayLayer = mipSubRange.baseArrayLayer;

            Utils::TransitionImageLayout(imageTransition);
        }

        VkBlitImageInfo2 imageBlitInfo{};
        imageBlitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        imageBlitInfo.filter = VK_FILTER_LINEAR;
        imageBlitInfo.dstImage = m_textureImage.image;
        imageBlitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBlitInfo.srcImage = m_textureImage.image;
        imageBlitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageBlitInfo.pRegions = &imageBlit;
        imageBlitInfo.regionCount = 1;

        vkCmdBlitImage2(blitCmd, &imageBlitInfo);

        {
            Utils::ImageTransitionInfo imageTransition{};
            imageTransition.image = m_textureImage.image;
            imageTransition.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageTransition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageTransition.cmd = blitCmd;
            imageTransition.logicalDevice = m_logicalDevice;
            imageTransition.layerCount = mipSubRange.layerCount;
            imageTransition.levelCount = mipSubRange.levelCount;
            imageTransition.baseMipLevel = mipSubRange.baseMipLevel;
            imageTransition.baseArrayLayer = mipSubRange.baseArrayLayer;

            Utils::TransitionImageLayout(imageTransition);
        }
    }

    subresourceRange.levelCount = mipLevels;
    {
        Utils::ImageTransitionInfo info{};
        info.image = m_textureImage.image;
        info.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        info.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.baseMipLevel = subresourceRange.baseMipLevel;
        info.levelCount = subresourceRange.levelCount;
        info.layerCount = subresourceRange.layerCount;
        info.cmd = blitCmd;
        info.logicalDevice = m_logicalDevice;

        Utils::TransitionImageLayout(info);
    }
    m_textureImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    m_logicalDevice->EndSingleTimeCommands(blitCmd);

    SamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.minFilter = textureSampler.minFilter;
    samplerCreateInfo.magFilter = textureSampler.magFilter;
    samplerCreateInfo.addressModeU = textureSampler.addressModeU;
    samplerCreateInfo.addressModeV = textureSampler.addressModeV;
    samplerCreateInfo.addressModeW = textureSampler.addressModeW;

    CreateTextureImageSampler(samplerCreateInfo);

    if(deleteBuffer) { delete[] buffer; }
}

void Texture::CreateTextureImage(const std::string& imagePath, const ImageType& imageType)
{
    SamplerCreateInfo samplerInfo{};

    if(imageType == ImageType::TEX2D)
    {
        gli::texture2d tex2D(gli::load(imagePath.c_str()));

        HGASSERT(!tex2D.empty() && "Failed to load texture image");

        width = static_cast<u32>(tex2D[0].extent().x);
        height = static_cast<u32>(tex2D[0].extent().y);
        mipLevels = static_cast<u32>(tex2D.levels());

        Buffer stagingBuffer{m_logicalDevice,
                             tex2D.size(),
                             1,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU};
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)tex2D.data(), tex2D.size());

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        size_t                         offset = 0;

        for(u32 i = 0; i < mipLevels; i++)
        {
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = i;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = static_cast<u32>(tex2D[i].extent().x);
            bufferCopyRegion.imageExtent.height = static_cast<u32>(tex2D[i].extent().y);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);

            offset += static_cast<u32>(tex2D[i].size());
        }

        Utils::AllocatedImageCreateInfo createInfo{.logicalDevice = *m_logicalDevice, .allocatedImage = m_textureImage};
        createInfo.width = width;
        createInfo.height = height;
        createInfo.mipLevels = mipLevels;
        createInfo.layerCount = 1;
        createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        createInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        createInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

        Utils::CreateAllocatedImage(createInfo);

        VkCommandBuffer cmd = m_logicalDevice->BeginSingleTimeCommands();

        Utils::ImageTransitionInfo first{};
        first.cmd = cmd;
        first.image = m_textureImage.image;
        first.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        first.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        first.logicalDevice = m_logicalDevice;
        first.baseMipLevel = 0;
        first.levelCount = mipLevels;
        first.baseArrayLayer = 0;
        first.layerCount = 1;

        Utils::TransitionImageLayout(first);

        m_logicalDevice->EndSingleTimeCommands(cmd);
        m_textureImage.imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        Utils::CopyBufferToImage(*m_logicalDevice, stagingBuffer.GetBuffer(), m_textureImage.image, bufferCopyRegions);

        VkCommandBuffer cmd2 = m_logicalDevice->BeginSingleTimeCommands();

        Utils::ImageTransitionInfo second{};
        second.cmd = cmd2;
        second.image = m_textureImage.image;
        second.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        second.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        second.logicalDevice = m_logicalDevice;
        second.baseMipLevel = 0;
        second.levelCount = mipLevels;
        second.baseArrayLayer = 0;
        second.layerCount = 1;

        Utils::TransitionImageLayout(second);

        m_logicalDevice->EndSingleTimeCommands(cmd2);
        m_textureImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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
        stagingBuffer.WriteToBuffer((u8*)texCube.data());

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
        // createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        createInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        createInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.width = static_cast<u32>(width);
        createInfo.height = static_cast<u32>(height);
        createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        createInfo.imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;

        Utils::CreateAllocatedImage(createInfo);

        VkCommandBuffer cmd = m_logicalDevice->BeginSingleTimeCommands();

        Utils::ImageTransitionInfo first{};
        first.cmd = cmd;
        first.image = m_textureImage.image;
        first.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        first.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        first.logicalDevice = m_logicalDevice;
        first.baseMipLevel = 0;
        first.levelCount = mipLevels;
        first.baseArrayLayer = 0;
        first.layerCount = 6;

        Utils::TransitionImageLayout(first);

        m_logicalDevice->EndSingleTimeCommands(cmd);
        m_textureImage.imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        Utils::CopyBufferToImage(*m_logicalDevice, stagingBuffer.GetBuffer(), m_textureImage.image, bufferCopyRegions);

        VkCommandBuffer cmd2 = m_logicalDevice->BeginSingleTimeCommands();

        Utils::ImageTransitionInfo second{};
        second.cmd = cmd;
        second.image = m_textureImage.image;
        second.oldLayout = m_textureImage.imageLayout;
        second.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        second.logicalDevice = m_logicalDevice;
        second.baseMipLevel = 0;
        second.levelCount = mipLevels;
        second.baseArrayLayer = 0;
        second.layerCount = 6;

        Utils::TransitionImageLayout(second);

        m_logicalDevice->EndSingleTimeCommands(cmd2);
        m_textureImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

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
    samplerInfo.maxAnisotropy =
        samplerInfo.anisotropyEnable ? m_logicalDevice->GetPhysicalDevice().GetProperties().properties.limits.maxSamplerAnisotropy : 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
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
