#pragma once

#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool_growable.hpp"
#include "logical_device.hpp"
#include "texture.hpp"
#include <string>

namespace Humongous
{

struct SkyboxCreateInfo
{
    LogicalDevice*          logicalDevice;
    const std::string&      cubemapPath;
    DescriptorSetLayout&    descriptorSetLayout;
    DescriptorPoolGrowable& imagePool;
    DescriptorPoolGrowable& uniformPool;
};

class Skybox
{
public:
    Skybox(const SkyboxCreateInfo& createInfo);
    ~Skybox();

    VkDescriptorSet GetDescriptorSet() const { return m_cubeMapSet; }
    VkDeviceAddress GetVertexBufferAddress() const { return m_vertexBuffer->GetDeviceAddress(); }

    void Draw(VkCommandBuffer cmd)
    {
        vkCmdBindIndexBuffer(cmd, m_indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, m_indirectDrawBuffer->GetBuffer(), 0, 1, sizeof(VkDrawIndexedIndirectCommand));
    }

private:
    LogicalDevice* m_logicalDevice = nullptr;

    std::unique_ptr<Texture> m_skybox;
    std::unique_ptr<Texture> m_irradiance;
    std::unique_ptr<Texture> m_brdflut;
    std::unique_ptr<Texture> m_prefilteredMap;
    std::vector<VkImageView> m_prefilteredMipViews;

    n32 m_vertexCount;
    n32 m_indexCount;

    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;

    std::unique_ptr<Buffer>      m_indirectDrawBuffer;
    VkDrawIndexedIndirectCommand m_command;
    VkDescriptorSet              m_cubeMapSet;

    void LoadCube();
    void LoadCubemap(const std::string& cubemapPath);
    void LoadDescriptorSet(DescriptorSetLayout& descriptorLayout, DescriptorPoolGrowable* pool);

    void GeneratePBRImages(DescriptorPoolGrowable& pool);
    void CreatePrefilteredMipViews();
};
} // namespace Humongous
