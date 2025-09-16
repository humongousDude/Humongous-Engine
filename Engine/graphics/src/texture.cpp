#define STB_IMAGE_IMPLEMENTATION
#include "texture.hpp"
#include "abstractions/buffer.hpp"
#include "allocator.hpp"
#include "asserts.hpp"
#include "defines.hpp"
#include "images.hpp"
#include "logger.hpp"
#include <ktx.h>
#include <ktxvulkan.h>
#include <stb_image.h>
#include <tiny_gltf.h>

namespace Humongous
{

Texture::Texture(const ILogicalDevice& logicalDevice, const std::string& imagePath, const ImageType& imageType, const bool& storage)
    : m_logicalDevice{logicalDevice}
{
    if(imagePath.empty())
    {
        HGWARN("Attempted to create a texture with an empty path");
        return;
    }

    CreateFromFile(imagePath, logicalDevice, imageType, storage);
}

void Texture::Destroy()
{
    if(m_textureSampler) { m_logicalDevice.DestroySampler(m_textureSampler); }
    if(m_textureImage.imageView) { m_logicalDevice.DestroyImageView(m_textureImage.imageView); }
    if(m_textureImage.image) { m_logicalDevice.GetAllocator().FreeImage(m_textureImage.allocation, m_textureImage.image); }
}

void Texture::CreateFromFile(const std::string& path, const ILogicalDevice& device, const ImageType& imageType, const bool& storage)
{
    CreateTextureImage(path, imageType, storage);
}
void Texture::GenerateMipmaps(vk::CommandBuffer commandBuffer, vk::Image image, u32 texWidth, u32 texHeight, u32 mipLevels,
                              vk::ImageLayout finalLayout)
{
    s32 mipWidth = texWidth;
    s32 mipHeight = texHeight;

    for(u32 i = 1; i < mipLevels; i++)
    {
        Utils::ImageTransitionInfo srcTransition{.cmd = commandBuffer,
                                                 .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                                                 .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                                                 .logicalDevice = m_logicalDevice,
                                                 .image = image,
                                                 .baseMipLevel = i - 1,
                                                 .levelCount = 1,
                                                 .layerCount = 1};
        Utils::TransitionImageLayout(srcTransition);

        vk::ImageBlit blit{};
        blit.srcOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.srcOffsets[1] = vk::Offset3D{mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.dstOffsets[1] = vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, blit,
                                vk::Filter::eLinear);

        Utils::ImageTransitionInfo finalSrcTransition{.cmd = commandBuffer,
                                                      .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
                                                      .newLayout = finalLayout,
                                                      .logicalDevice = m_logicalDevice,
                                                      .image = image,
                                                      .baseMipLevel = i - 1,
                                                      .levelCount = 1,
                                                      .layerCount = 1};
        Utils::TransitionImageLayout(finalSrcTransition);

        if(mipWidth > 1) { mipWidth /= 2; }
        if(mipHeight > 1) { mipHeight /= 2; }
    }

    Utils::ImageTransitionInfo finalDstTransition{.cmd = commandBuffer,
                                                  .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                                                  .newLayout = finalLayout,
                                                  .logicalDevice = m_logicalDevice,
                                                  .image = image,
                                                  .baseMipLevel = mipLevels - 1,
                                                  .levelCount = 1,
                                                  .layerCount = 1};
    Utils::TransitionImageLayout(finalDstTransition);
}

void Texture::CreateFromGLTFImage(tinygltf::Image& gltfimage, TexSamplerInfo textureSampler, const ILogicalDevice& device, vk::Queue copyQueue)
{
    unsigned char* buffer = nullptr;
    vk::DeviceSize bufferSize = 0;
    bool           deleteBuffer = false;

    m_width = gltfimage.width;
    m_height = gltfimage.height;
    vk::Format format = vk::Format::eR8G8B8A8Unorm;
    size_t     expectedBufferSize = m_width * m_height * 4;

    if(gltfimage.component == 3)
    {
        bufferSize = expectedBufferSize;
        buffer = new unsigned char[bufferSize];
        for(s32 i = 0; i < m_width * m_height; ++i)
        {
            auto rgba = buffer + i * 4;
            auto rgb = &gltfimage.image[i * 3];
            rgba[0] = rgb[0];
            rgba[1] = rgb[1];
            rgba[2] = rgb[2];
            rgba[3] = 255;
        }
        deleteBuffer = true;
    }
    else if(gltfimage.component == 4)
    {
        bufferSize = gltfimage.image.size();
        buffer = gltfimage.image.data();
    }
    else if(gltfimage.component == 1)
    {
        bufferSize = expectedBufferSize;
        buffer = new unsigned char[bufferSize];
        for(s32 i = 0; i < m_width * m_height; ++i)
        {
            unsigned char grey = gltfimage.image[i];
            auto          rgba = buffer + i * 4;
            rgba[0] = grey;
            rgba[1] = grey;
            rgba[2] = grey;
            rgba[3] = 255;
        }
        deleteBuffer = true;
    }
    else if(gltfimage.component == 2)
    {
        bufferSize = expectedBufferSize;
        buffer = new unsigned char[bufferSize];
        for(s32 i = 0; i < m_width * m_height; ++i)
        {
            auto ga = &gltfimage.image[i * 2];
            auto rgba = buffer + i * 4;
            rgba[0] = ga[0];
            rgba[1] = ga[0];
            rgba[2] = ga[0];
            rgba[3] = ga[1];
        }
        deleteBuffer = true;
    }
    else
    {
        HGASSERT(false && "Unsupported image component count");
        return;
    }

    m_miplevels = static_cast<u32>(std::floor(std::log2(std::max(m_width, m_height))) + 1.0f);

    if(m_logicalDevice.GetPhysicalDevice().GetVkPhysicalDevice() == VK_NULL_HANDLE)
    {
        HGERROR("Unable to create texture image, physical device is null");
        return;
    }

    vk::FormatProperties fmtProps = m_logicalDevice.GetPhysicalDevice().GetVkPhysicalDevice().getFormatProperties(format);
    HGASSERT(fmtProps.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc);
    HGASSERT(fmtProps.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst);

    Buffer::BufferCreateInfo bufCreateInfo{.device = m_logicalDevice};
    bufCreateInfo.size = bufferSize;
    bufCreateInfo.instanceCount = 1;
    bufCreateInfo.bufferUsage = vk::BufferUsageFlagBits::eTransferSrc;
    bufCreateInfo.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    bufCreateInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    bufCreateInfo.minOffsetAlignment = 1;
    bufCreateInfo.name = "staging buffer";

    Buffer stagingBuffer{bufCreateInfo};
    stagingBuffer.Map();
    stagingBuffer.WriteToBuffer(buffer, bufferSize);

    Utils::AllocatedImageCreateInfo createInfo{.logicalDevice = m_logicalDevice, .allocatedImage = &m_textureImage};
    createInfo.width = m_width;
    createInfo.height = m_height;
    createInfo.mipLevels = m_miplevels;
    createInfo.layerCount = 1;
    createInfo.format = format;
    createInfo.tiling = vk::ImageTiling::eOptimal;
    createInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;
    createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    createInfo.aspectFlags = vk::ImageAspectFlagBits::eColor;
    createInfo.flags = {};
    createInfo.imageViewType = vk::ImageViewType::e2D;
    createInfo.name = gltfimage.name;

    Utils::CreateAllocatedImage(createInfo);

    Utils::TransitionImageLayout(m_logicalDevice, m_textureImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    m_textureImage.imageLayout = vk::ImageLayout::eTransferDstOptimal;

    vk::BufferImageCopy region{};
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = VkExtent3D(m_width, m_height, 1);

    Utils::CopyBufferToImage(m_logicalDevice, stagingBuffer.GetBuffer(), m_textureImage.image, std::vector<vk::BufferImageCopy>{region});

    Utils::TransitionImageLayout(m_logicalDevice, m_textureImage.image, m_textureImage.imageLayout, vk::ImageLayout::eTransferSrcOptimal);
    m_textureImage.imageLayout = vk::ImageLayout::eTransferSrcOptimal;

    vk::CommandBuffer blitCmd = m_logicalDevice.BeginSingleTimeCommands();

    vk::ImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;

    for(u32 i = 1; i < m_miplevels; i++)
    {
        vk::ImageBlit2 imageBlit{};

        imageBlit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageBlit.srcSubresource.layerCount = 1;
        imageBlit.srcSubresource.mipLevel = i - 1;
        imageBlit.srcOffsets[1].x = s32(m_width >> (i - 1));
        imageBlit.srcOffsets[1].y = s32(m_height >> (i - 1));
        imageBlit.srcOffsets[1].z = 1;

        imageBlit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageBlit.dstSubresource.layerCount = 1;
        imageBlit.dstSubresource.mipLevel = i;
        imageBlit.dstOffsets[1].x = s32(m_width >> i);
        imageBlit.dstOffsets[1].y = s32(m_height >> i);
        imageBlit.dstOffsets[1].z = 1;

        vk::ImageSubresourceRange mipSubRange = {};
        mipSubRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        mipSubRange.baseMipLevel = i;
        mipSubRange.levelCount = 1;
        mipSubRange.layerCount = 1;

        {
            Utils::ImageTransitionInfo imageTransition{.logicalDevice = m_logicalDevice};
            imageTransition.image = m_textureImage.image;
            imageTransition.oldLayout = vk::ImageLayout::eUndefined;
            imageTransition.newLayout = vk::ImageLayout::eTransferDstOptimal;
            imageTransition.cmd = blitCmd;
            imageTransition.layerCount = mipSubRange.layerCount;
            imageTransition.levelCount = mipSubRange.levelCount;
            imageTransition.baseMipLevel = mipSubRange.baseMipLevel;
            imageTransition.baseArrayLayer = mipSubRange.baseArrayLayer;

            Utils::TransitionImageLayout(imageTransition);
        }

        vk::BlitImageInfo2 imageBlitInfo{};
        imageBlitInfo.sType = vk::StructureType::eBlitImageInfo2;
        imageBlitInfo.filter = vk::Filter::eLinear;
        imageBlitInfo.srcImage = m_textureImage.image;
        imageBlitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
        imageBlitInfo.dstImage = m_textureImage.image;
        imageBlitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
        imageBlitInfo.pRegions = &imageBlit;
        imageBlitInfo.regionCount = 1;

        blitCmd.blitImage2(&imageBlitInfo);

        {
            Utils::ImageTransitionInfo imageTransition{.logicalDevice = m_logicalDevice};
            imageTransition.image = m_textureImage.image;
            imageTransition.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            imageTransition.newLayout = vk::ImageLayout::eTransferSrcOptimal;
            imageTransition.cmd = blitCmd;
            imageTransition.layerCount = mipSubRange.layerCount;
            imageTransition.levelCount = mipSubRange.levelCount;
            imageTransition.baseMipLevel = mipSubRange.baseMipLevel;
            imageTransition.baseArrayLayer = mipSubRange.baseArrayLayer;

            Utils::TransitionImageLayout(imageTransition);
        }
    }

    subresourceRange.levelCount = m_miplevels;
    {
        Utils::ImageTransitionInfo info{.logicalDevice = m_logicalDevice};
        info.image = m_textureImage.image;
        info.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        info.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        info.baseMipLevel = subresourceRange.baseMipLevel;
        info.levelCount = subresourceRange.levelCount;
        info.layerCount = subresourceRange.layerCount;
        info.cmd = blitCmd;

        Utils::TransitionImageLayout(info);
    }

    m_textureImage.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    m_logicalDevice.EndSingleTimeCommands(blitCmd);

    TexSamplerInfo samplerInfo{};
    samplerInfo.minFilter = textureSampler.minFilter;
    samplerInfo.magFilter = textureSampler.magFilter;
    samplerInfo.addressModeU = textureSampler.addressModeU;
    samplerInfo.addressModeV = textureSampler.addressModeV;
    samplerInfo.addressModeW = textureSampler.addressModeW;
    CreateTextureImageSampler(samplerInfo);

    if(deleteBuffer) { delete[] buffer; }
}

void Texture::CreateTextureImage(const std::string& imagePath, const ImageType& imageType, const bool& storage)
{
    TexSamplerInfo samplerInfo{};

    if(imageType == ImageType::TEX2D)
    {
        s32      texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(imagePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if(!pixels)
        {
            HGERROR("Failed to load texture image!");
            return;
        }

        m_width = static_cast<u32>(texWidth);
        m_height = static_cast<u32>(texHeight);
        m_baseSize = m_width;
        vk::DeviceSize imageSize = m_width * m_height * 4;

        m_miplevels = static_cast<u32>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;

        Buffer::BufferCreateInfo bufCreateInfo{.device = m_logicalDevice};
        bufCreateInfo.size = imageSize;
        bufCreateInfo.instanceCount = 1;
        bufCreateInfo.bufferUsage = vk::BufferUsageFlagBits::eTransferSrc;
        bufCreateInfo.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufCreateInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        bufCreateInfo.minOffsetAlignment = 1;
        bufCreateInfo.name = "staging buffer";

        Buffer stagingBuffer{bufCreateInfo};
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer(pixels, static_cast<size_t>(imageSize));

        stbi_image_free(pixels);

        Utils::AllocatedImageCreateInfo createInfo{.logicalDevice = m_logicalDevice, .allocatedImage = &m_textureImage};
        createInfo.width = m_width;
        createInfo.height = m_height;
        createInfo.mipLevels = m_miplevels;
        createInfo.layerCount = 1;
        createInfo.format = vk::Format::eR8G8B8A8Unorm;
        createInfo.tiling = vk::ImageTiling::eOptimal;

        size_t lastSlashPos = imagePath.rfind('/');
        size_t lastBackslashPos = imagePath.rfind('\\');

        size_t extpos = imagePath.rfind('.', imagePath.length());
        size_t filenameStartPos = std::string::npos;
        if(lastSlashPos != std::string::npos && lastBackslashPos != std::string::npos)
        {
            filenameStartPos = std::max(lastSlashPos, lastBackslashPos);
        }
        else if(lastSlashPos != std::string::npos) { filenameStartPos = lastSlashPos; }
        else if(lastBackslashPos != std::string::npos) { filenameStartPos = lastBackslashPos; }

        std::string fileName;
        if(filenameStartPos != std::string::npos) { fileName = imagePath.substr(filenameStartPos + 1); }
        else { fileName = imagePath; }

        std::string filenameWithoutExtension;
        if(extpos != std::string::npos && extpos > filenameStartPos)
        {
            if(filenameStartPos != std::string::npos)
            {
                filenameWithoutExtension = imagePath.substr(filenameStartPos + 1, extpos - (filenameStartPos + 1));
            }
            else { filenameWithoutExtension = imagePath.substr(0, extpos); }
        }
        else { filenameWithoutExtension = fileName; }

        std::string name = filenameWithoutExtension;
        createInfo.name = name.c_str();

        vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        if(storage) { usage |= vk::ImageUsageFlagBits::eStorage; }

        createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        createInfo.aspectFlags = vk::ImageAspectFlagBits::eColor;

        Utils::CreateAllocatedImage(createInfo);

        auto cmd = m_logicalDevice.BeginSingleTimeCommands();
        if(cmd == VK_NULL_HANDLE)
        {
            HGERROR("Unable to create texture image, logical device failed to provide one-time submit command buffer");
            return;
        }

        Utils::ImageTransitionInfo initialTransitionInfo{.cmd = cmd,
                                                         .oldLayout = vk::ImageLayout::eUndefined,
                                                         .newLayout = vk::ImageLayout::eTransferDstOptimal,
                                                         .logicalDevice = m_logicalDevice,
                                                         .image = m_textureImage.image,
                                                         .baseMipLevel = 0,
                                                         .levelCount = m_miplevels};
        Utils::TransitionImageLayout(initialTransitionInfo);

        vk::BufferImageCopy region{};
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = vk::Extent3D{m_width, m_height, 1};
        Utils::CopyBufferToImage(m_logicalDevice, stagingBuffer.GetBuffer(), m_textureImage.image, {region});

        GenerateMipmaps(cmd, m_textureImage.image, m_width, m_height, m_miplevels, vk::ImageLayout::eShaderReadOnlyOptimal);

        m_logicalDevice.EndSingleTimeCommands(cmd);

        m_textureImage.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        if(storage) { m_textureImage.imageLayout = vk::ImageLayout::eGeneral; }

        samplerInfo.magFilter = vk::Filter::eLinear;
        samplerInfo.minFilter = vk::Filter::eLinear;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        CreateTextureImageSampler(samplerInfo);
    }
    else if(imageType == ImageType::CUBEMAP)
    {
        ktxTexture*    ktxTex;
        KTX_error_code result;

        result = ktxTexture_CreateFromNamedFile(imagePath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex);
        if(result != KTX_SUCCESS)
        {
            HGERROR("Failed to load KTX texture file.");
            return;
        }

        m_width = ktxTex->baseWidth;
        m_height = ktxTex->baseHeight;
        m_miplevels = ktxTex->numLevels;
        m_baseSize = m_width;
        vk::Format format = vk::Format(ktxTexture_GetVkFormat(ktxTex));

        Buffer::BufferCreateInfo bufCreateInfo{.device = m_logicalDevice};
        bufCreateInfo.size = ktxTexture_GetDataSize(ktxTex);
        bufCreateInfo.instanceCount = 1;
        bufCreateInfo.bufferUsage = vk::BufferUsageFlagBits::eTransferSrc;
        bufCreateInfo.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufCreateInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        bufCreateInfo.minOffsetAlignment = 1;
        bufCreateInfo.name = "staging buffer";

        Buffer stagingBuffer{bufCreateInfo};
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer(ktxTexture_GetData(ktxTex), ktxTexture_GetDataSize(ktxTex));

        std::vector<vk::BufferImageCopy> regions;
        for(u32 level = 0; level < m_miplevels; ++level)
        {
            for(u32 face = 0; face < ktxTex->numFaces; ++face)
            {
                ktx_size_t offset;
                result = ktxTexture_GetImageOffset(ktxTex, level, 0, face, &offset);
                HGASSERT(result == KTX_SUCCESS);

                vk::BufferImageCopy region{};
                region.bufferOffset = offset;
                region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                region.imageSubresource.mipLevel = level;
                region.imageSubresource.baseArrayLayer = face;
                region.imageSubresource.layerCount = 1;
                region.imageExtent.width = std::max(1u, ktxTex->baseWidth >> level);
                region.imageExtent.height = std::max(1u, ktxTex->baseHeight >> level);
                region.imageExtent.depth = 1;
                regions.push_back(region);
            }
        }

        Utils::AllocatedImageCreateInfo createInfo{.logicalDevice = m_logicalDevice, .allocatedImage = &m_textureImage};
        createInfo.width = m_width;
        createInfo.height = m_height;
        createInfo.mipLevels = m_miplevels;
        createInfo.layerCount = 6;
        createInfo.format = vk::Format::eR16G16B16A16Sfloat;
        createInfo.tiling = vk::ImageTiling::eOptimal;
        createInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled |
                           (storage ? vk::ImageUsageFlagBits::eStorage : vk::ImageUsageFlagBits());
        createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        createInfo.aspectFlags = vk::ImageAspectFlagBits::eColor;
        createInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
        createInfo.imageViewType = vk::ImageViewType::eCube;
        createInfo.name = "temp cube";

        Utils::CreateAllocatedImage(createInfo);

        auto cmd = m_logicalDevice.BeginSingleTimeCommands();
        if(cmd == VK_NULL_HANDLE)
        {
            HGERROR("Unable to create texture image, logical device failed to provide one-time submit command buffer");
            return;
        }

        Utils::ImageTransitionInfo first{.logicalDevice = m_logicalDevice};
        first.cmd = cmd;
        first.image = m_textureImage.image;
        first.oldLayout = vk::ImageLayout::eUndefined;
        first.newLayout = vk::ImageLayout::eTransferDstOptimal;
        first.baseMipLevel = 0;
        first.levelCount = m_miplevels;
        first.baseArrayLayer = 0;
        first.layerCount = 6;

        Utils::TransitionImageLayout(first);

        m_logicalDevice.EndSingleTimeCommands(cmd);
        m_textureImage.imageLayout = vk::ImageLayout::eTransferDstOptimal;

        Utils::CopyBufferToImage(m_logicalDevice, stagingBuffer.GetBuffer(), m_textureImage.image, regions);

        vk::CommandBuffer cmd2 = m_logicalDevice.BeginSingleTimeCommands();
        if(cmd2 == VK_NULL_HANDLE)
        {
            HGERROR("Unable to create texture image, logical device failed to provide one-time submit command buffer");
            return;
        }

        Utils::ImageTransitionInfo second{.logicalDevice = m_logicalDevice};
        second.cmd = cmd;
        second.image = m_textureImage.image;
        second.oldLayout = m_textureImage.imageLayout;
        if(storage) { second.newLayout = vk::ImageLayout::eGeneral; }
        else { second.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal; }
        second.baseMipLevel = 0;
        second.levelCount = m_miplevels;
        second.baseArrayLayer = 0;
        second.layerCount = 6;

        Utils::TransitionImageLayout(second);

        m_logicalDevice.EndSingleTimeCommands(cmd2);
        if(storage) { m_textureImage.imageLayout = vk::ImageLayout::eGeneral; }
        else { m_textureImage.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal; }

        samplerInfo.magFilter = vk::Filter::eLinear;
        samplerInfo.minFilter = vk::Filter::eLinear;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;

        CreateTextureImageSampler(samplerInfo, ImageType::CUBEMAP);
    }
}

void Texture::FillWithEmpty(const ILogicalDevice& logicalDevice, u32 width, u32 height, const bool& storage)
{
    m_width = width;
    m_height = height;
    m_miplevels = 1;
    m_layerCount = 1;

    Utils::AllocatedImageCreateInfo createInfo{.logicalDevice = logicalDevice, .allocatedImage = &m_textureImage};
    createInfo.width = width;
    createInfo.height = height;
    createInfo.mipLevels = 1;
    createInfo.layerCount = 1;
    createInfo.format = vk::Format::eR16G16B16A16Sfloat;
    createInfo.tiling = vk::ImageTiling::eOptimal;
    createInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled |
                       (storage ? vk::ImageUsageFlagBits::eStorage : vk::ImageUsageFlagBits());
    createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    createInfo.aspectFlags = vk::ImageAspectFlagBits::eColor;

    std::string name = "empty image " + std::to_string(width) + std::to_string(height);
    createInfo.name = name;

    Utils::CreateAllocatedImage(createInfo);

    vk::CommandBuffer cmd = logicalDevice.BeginSingleTimeCommands();

    Utils::ImageTransitionInfo transinfo{.logicalDevice = logicalDevice};
    transinfo.cmd = cmd;
    transinfo.image = m_textureImage.image;
    transinfo.oldLayout = vk::ImageLayout::eUndefined;
    if(storage) { transinfo.newLayout = vk::ImageLayout::eGeneral; }
    else { transinfo.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal; }
    transinfo.baseMipLevel = 0;
    transinfo.levelCount = 1;
    transinfo.baseArrayLayer = 0;
    transinfo.layerCount = 1;

    Utils::TransitionImageLayout(transinfo);

    logicalDevice.EndSingleTimeCommands(cmd);

    m_textureImage.imageLayout = storage ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;

    TexSamplerInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    CreateTextureImageSampler(samplerInfo);
}

void Texture::CreateTextureImageSampler(const TexSamplerInfo& info, const ImageType& imageType)
{
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = info.magFilter;
    samplerInfo.minFilter = info.minFilter;
    samplerInfo.addressModeU = info.addressModeU;
    samplerInfo.addressModeV = info.addressModeV;
    samplerInfo.addressModeW = info.addressModeW;
    samplerInfo.anisotropyEnable = m_logicalDevice.GetPhysicalDevice().GetFeatures().features.samplerAnisotropy;
    samplerInfo.maxAnisotropy =
        samplerInfo.anisotropyEnable ? m_logicalDevice.GetPhysicalDevice().GetProperties().properties.limits.maxSamplerAnisotropy : 1.0f;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueWhite;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = vk::CompareOp::eNever;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(m_miplevels);

    m_textureSampler = m_logicalDevice.CreateSampler(samplerInfo);
}

} // namespace Humongous
