#pragma once

#include "logical_device.hpp"
#include <abstractions/buffer.hpp>
#include <renderer.hpp>
#include <string>

namespace Humongous
{
class Texture
{
public:
    Texture(LogicalDevice& m_logicalDevice, const std::string& imagePath);
    ~Texture();

    VkDescriptorImageInfo GetDescriptorInfo() const
    {
        return {m_textureSampler, m_textureImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    };

private:
    LogicalDevice& m_logicalDevice;
    AllocatedImage m_textureImage;
    VkSampler      m_textureSampler;

    void CreateTextureImage(const std::string& imagePath);
    void CreateTextureImageSampler();
};
}; // namespace Humongous
