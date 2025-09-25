#define STB_IMAGE_IMPLEMENTATION
#include "texture.hpp"
#include "abstractions/buffer.hpp"
#include "allocator.hpp"
#include "asserts.hpp"
#include "defines.hpp"
#include "logger.hpp"
#include <ktx.h>
#include <ktxvulkan.h>
#include <stb_image.h>
#include <tiny_gltf.h>

namespace Humongous
{

Texture::Texture(const ILogicalDevice& logicalDevice, const std::string& imagePath, const ImageType& imageType, const b8& storage)
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
}

void Texture::CreateFromFile(const std::string& path, const ILogicalDevice& device, const ImageType& imageType, const b8& storage)
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
        m_textureImage->TransitionLayout(commandBuffer, vk::ImageLayout::eTransferSrcOptimal, i - 1, 1, 0, 1);

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

        m_textureImage->TransitionLayout(commandBuffer, finalLayout, i - 1, 1, 0, 1);

        if(mipWidth > 1) { mipWidth /= 2; }
        if(mipHeight > 1) { mipHeight /= 2; }
    }

    m_textureImage->TransitionLayout(commandBuffer, finalLayout, mipLevels - 1, 1, 0, 1);
}

void Texture::CreateFromGLTFImage(tinygltf::Image& gltfimage, TexSamplerInfo textureSampler, const ILogicalDevice& device, vk::Queue copyQueue)
{
    unsigned char* buffer = nullptr;
    vk::DeviceSize bufferSize = 0;
    b8             deleteBuffer = false;

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

    // Do we need this?
    vk::FormatProperties fmtProps = m_logicalDevice.GetFormatProperties(format);

    if(!(fmtProps.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc) ||
       !(fmtProps.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst))
    {
        HGERROR("Texture image format does not support blitting! Defaulting image format");
        format = vk::Format::eR8G8B8A8Unorm;
    }

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

    Image::ImageCreateInfo createInfo{};
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

    m_textureImage = std::make_unique<Image>(m_logicalDevice, createInfo);

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();

    m_textureImage->TransitionLayout(cmd, vk::ImageLayout::eTransferDstOptimal);

    vk::BufferImageCopy region{};
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = VkExtent3D(m_width, m_height, 1);

    stagingBuffer.CopyToImage(cmd, *m_textureImage, {region});

    m_textureImage->TransitionLayout(cmd, vk::ImageLayout::eTransferSrcOptimal);

    m_logicalDevice.EndSingleTimeCommands(cmd);

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

        m_textureImage->TransitionLayout(blitCmd, vk::ImageLayout::eTransferDstOptimal, mipSubRange.baseMipLevel, mipSubRange.levelCount,
                                         mipSubRange.baseArrayLayer, mipSubRange.layerCount);

        vk::BlitImageInfo2 imageBlitInfo{};
        imageBlitInfo.sType = vk::StructureType::eBlitImageInfo2;
        imageBlitInfo.filter = vk::Filter::eLinear;
        imageBlitInfo.srcImage = m_textureImage->GetImage();
        imageBlitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
        imageBlitInfo.dstImage = m_textureImage->GetImage();
        imageBlitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
        imageBlitInfo.pRegions = &imageBlit;
        imageBlitInfo.regionCount = 1;

        m_logicalDevice.RecordBlitCommand(blitCmd, imageBlitInfo);

        m_textureImage->TransitionLayout(blitCmd, vk::ImageLayout::eTransferSrcOptimal, mipSubRange.baseMipLevel, mipSubRange.levelCount,
                                         mipSubRange.baseArrayLayer, mipSubRange.layerCount);
    }

    subresourceRange.levelCount = m_miplevels;
    m_textureImage->TransitionLayout(blitCmd, vk::ImageLayout::eShaderReadOnlyOptimal, subresourceRange.baseMipLevel, subresourceRange.levelCount,
                                     subresourceRange.baseArrayLayer, subresourceRange.layerCount);

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

void Texture::CreateTextureImage(const std::string& imagePath, const ImageType& imageType, const b8& storage)
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

        Image::ImageCreateInfo createInfo{};
        createInfo.width = m_width;
        createInfo.height = m_height;
        createInfo.mipLevels = m_miplevels;
        createInfo.layerCount = 1;
        createInfo.format = vk::Format::eR8G8B8A8Unorm;
        createInfo.tiling = vk::ImageTiling::eOptimal;
        createInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        createInfo.aspectFlags = vk::ImageAspectFlagBits::eColor;
        createInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
        createInfo.imageViewType = vk::ImageViewType::eCube;

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
        else
        {
            fileName = imagePath;
        }

        std::string filenameWithoutExtension;
        if(extpos != std::string::npos && extpos > filenameStartPos)
        {
            if(filenameStartPos != std::string::npos)
            {
                filenameWithoutExtension = imagePath.substr(filenameStartPos + 1, extpos - (filenameStartPos + 1));
            }
            else
            {
                filenameWithoutExtension = imagePath.substr(0, extpos);
            }
        }
        else
        {
            filenameWithoutExtension = fileName;
        }

        std::string name = filenameWithoutExtension;
        createInfo.name = name.c_str();

        vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        if(storage) { usage |= vk::ImageUsageFlagBits::eStorage; }

        createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        createInfo.aspectFlags = vk::ImageAspectFlagBits::eColor;

        m_textureImage = std::make_unique<Image>(m_logicalDevice, createInfo);

        auto cmd = m_logicalDevice.BeginSingleTimeCommands();
        if(cmd == VK_NULL_HANDLE)
        {
            HGERROR("Unable to create texture image, logical device failed to provide one-time submit command buffer");
            return;
        }
        m_textureImage->TransitionLayout(cmd, vk::ImageLayout::eTransferDstOptimal);

        vk::BufferImageCopy region{};
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = vk::Extent3D{m_width, m_height, 1};
        stagingBuffer.CopyToImage(cmd, *m_textureImage, {region});

        GenerateMipmaps(cmd, m_textureImage->GetImage(), m_width, m_height, m_miplevels,
                        storage ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal);

        m_logicalDevice.GetWorkScheduler().AddWork(cmd, m_logicalDevice.GetTransferQueue());

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

        Image::ImageCreateInfo createInfo{};
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

        m_textureImage = std::make_unique<Image>(m_logicalDevice, createInfo);
        if(!m_textureImage || !m_textureImage->IsValid())
        {
            HGERROR("Failed to create texture image from file: {}", imagePath.c_str());
            ktxTexture_Destroy(ktxTex);
            return;
        }

        auto cmd = m_logicalDevice.BeginSingleTimeCommands();

        m_textureImage->TransitionLayout(cmd, vk::ImageLayout::eTransferDstOptimal);

        stagingBuffer.CopyToImage(cmd, *m_textureImage, regions);
        m_logicalDevice.EndSingleTimeCommands(cmd);

        vk::CommandBuffer cmd2 = m_logicalDevice.BeginSingleTimeCommands();

        m_textureImage->TransitionLayout(cmd2, storage ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal);

        m_logicalDevice.EndSingleTimeCommands(cmd2);

        samplerInfo.magFilter = vk::Filter::eLinear;
        samplerInfo.minFilter = vk::Filter::eLinear;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;

        CreateTextureImageSampler(samplerInfo, ImageType::CUBEMAP);

        ktxTexture_Destroy(ktxTex);
    }
}

void Texture::FillWithEmpty(const ILogicalDevice& logicalDevice, u32 width, u32 height, const b8& storage)
{
    m_width = width;
    m_height = height;
    m_miplevels = 1;
    m_layerCount = 1;

    Image::ImageCreateInfo createInfo{};
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

    m_textureImage = std::make_unique<Image>(logicalDevice, createInfo);

    vk::CommandBuffer cmd = logicalDevice.BeginSingleTimeCommands();

    m_textureImage->TransitionLayout(cmd, storage ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal);

    logicalDevice.EndSingleTimeCommands(cmd);

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
    samplerInfo.anisotropyEnable = m_logicalDevice.GetFeatures().features.samplerAnisotropy;
    samplerInfo.maxAnisotropy = samplerInfo.anisotropyEnable ? m_logicalDevice.GetProperties().properties.limits.maxSamplerAnisotropy : 1.0f;
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
