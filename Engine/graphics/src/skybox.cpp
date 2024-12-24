#include "skybox.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "extra.hpp"
#include "model.hpp"
#include "tiny_gltf.h"

namespace Humongous
{

Skybox::Skybox(const SkyboxCreateInfo& createInfo) : m_logicalDevice{createInfo.logicalDevice}
{
    LoadCube();
    LoadCubemap(createInfo.cubemapPath);
    LoadDescriptorSet(createInfo.descriptorSetLayout, &createInfo.growablePool);
}

Skybox::~Skybox() { m_skybox->Destroy(); }

void Skybox::LoadCube()
{
    // These are hardcoded, as we will always create a cube

    // source: https://pastebin.com/4T10MFgb
    std::vector<n32> indices = {0,  1,  2,  0,  3,  1,  4,  5,  6,  4,  7,  5,  8,  9,  10, 8,  11, 9,
                                12, 13, 14, 12, 15, 13, 16, 17, 18, 16, 19, 17, 20, 21, 22, 20, 23, 21};

    std::vector<Model::Vertex> vertices = {
        // left face (white)
        {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
        {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
        {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},
        {{-.5f, .5f, -.5f}, {.9f, .9f, .9f}},

        // right face (yellow)
        {{.5f, -.5f, -.5f}, {.8f, .8f, .1f}},
        {{.5f, .5f, .5f}, {.8f, .8f, .1f}},
        {{.5f, -.5f, .5f}, {.8f, .8f, .1f}},
        {{.5f, .5f, -.5f}, {.8f, .8f, .1f}},

        // top face (orange, remember y axis points down)
        {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
        {{.5f, -.5f, .5f}, {.9f, .6f, .1f}},
        {{-.5f, -.5f, .5f}, {.9f, .6f, .1f}},
        {{.5f, -.5f, -.5f}, {.9f, .6f, .1f}},

        // bottom face (red)
        {{-.5f, .5f, -.5f}, {.8f, .1f, .1f}},
        {{.5f, .5f, .5f}, {.8f, .1f, .1f}},
        {{-.5f, .5f, .5f}, {.8f, .1f, .1f}},
        {{.5f, .5f, -.5f}, {.8f, .1f, .1f}},

        // nose face (blue)
        {{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
        {{.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
        {{-.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
        {{.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},

        // tail face (green)
        {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
        {{.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
        {{-.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
        {{.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
    };

    // makes the skybox very far away
    for(auto& vert: vertices) { vert.position *= 1000; }

    m_vertexCount = static_cast<n32>(vertices.size());
    m_indexCount = static_cast<n32>(indices.size());

    // Vertex buffer
    {
        VkDeviceSize bufferSize = sizeof(Model::Vertex) * m_vertexCount;

        Buffer stagingBuffer{m_logicalDevice,
                             bufferSize,
                             1,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU};
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)vertices.data());

        m_vertexBuffer =
            std::make_unique<Buffer>(m_logicalDevice, bufferSize, 1, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, *m_vertexBuffer, bufferSize);
    }

    // Index Buffer
    {
        VkDeviceSize bufferSize = sizeof(n32) * m_indexCount;

        Buffer stagingBuffer{m_logicalDevice,
                             bufferSize,
                             1,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU};
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)indices.data());

        m_indexBuffer =
            std::make_unique<Buffer>(m_logicalDevice, bufferSize, 1, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_CPU_COPY);

        Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, *m_indexBuffer, bufferSize);
    }
}

void Skybox::LoadCubemap(const std::string& cubemapPath)
{
    m_skybox = std::make_unique<Texture>(m_logicalDevice, cubemapPath, Texture::ImageType::CUBEMAP);
}

void Skybox::LoadDescriptorSet(DescriptorSetLayout& descriptorLayout, DescriptorPoolGrowable* pool)
{
    auto imgInfo = m_skybox->GetDescriptorInfo();
    DescriptorWriter(descriptorLayout, pool).WriteImage(0, &imgInfo).Build(m_cubeMapSet);
}

} // namespace Humongous
