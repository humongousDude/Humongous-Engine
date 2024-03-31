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
    DescriptorPoolGrowable& growablePool;
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
        vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 1);
    }

private:
    LogicalDevice* m_logicalDevice = nullptr;

    std::unique_ptr<Texture> m_skybox;
    u32                      m_vertexCount;
    u32                      m_indexCount;
    std::unique_ptr<Buffer>  m_vertexBuffer;
    std::unique_ptr<Buffer>  m_indexBuffer;
    VkDescriptorSet          m_cubeMapSet;

    void LoadCube();
    void LoadCubemap(const std::string& cubemapPath);
    void LoadDescriptorSet(DescriptorSetLayout& descriptorLayout, DescriptorPoolGrowable* pool);
};
} // namespace Humongous
