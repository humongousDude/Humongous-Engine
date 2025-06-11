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
    DescriptorPoolGrowable& storageImagePool;
};

class Skybox
{
public:
    Skybox(const SkyboxCreateInfo& createInfo);
    ~Skybox();

    vk::DescriptorSet GetDescriptorSet() const { return m_cubeMapSet; }

    void Draw(vk::CommandBuffer cmd) { cmd.draw(6, 1, 0, 0); }

private:
    LogicalDevice* m_logicalDevice = nullptr;

    std::unique_ptr<Texture>   m_skybox;
    std::unique_ptr<Texture>   m_irradiance;
    std::unique_ptr<Texture>   m_brdflut;
    std::unique_ptr<Texture>   m_prefilteredMap;
    std::vector<vk::ImageView> m_prefilteredMipViews;

    n32 m_vertexCount;
    n32 m_indexCount;

    vk::DrawIndexedIndirectCommand m_command;
    vk::DescriptorSet              m_cubeMapSet;

    void LoadCube();
    void LoadCubemap(const std::string& cubemapPath);
    void LoadDescriptorSet(DescriptorSetLayout& descriptorLayout, DescriptorPoolGrowable* pool);

    void GeneratePBRImages(DescriptorPoolGrowable& uniformPool, DescriptorPoolGrowable& combinedImagePool,
                           DescriptorPoolGrowable& storageImagePool);
    void CreatePrefilteredMipViews();
};
} // namespace Humongous
