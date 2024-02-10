#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "logger.hpp"
#include <model.hpp>

namespace Humongous
{
Model::Model(LogicalDevice& device, const std::vector<Vertex>& vertices, const std::vector<u32>& indices, const std::string& imagePath)
    : m_logicalDevice{device}
{
    CreateVertexBuffer(vertices);
    CreateIndexBuffer(indices);
    m_texture = std::make_unique<Texture>(m_logicalDevice, imagePath);
}

Model::~Model() {}

void Model::CreateVertexBuffer(const std::vector<Vertex>& vertices)
{
    m_vertexCount = static_cast<u32>(vertices.size());
    HGASSERT(m_vertexCount >= 3);

    VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();
    u32          vertexSize = sizeof(Vertex);

    Buffer stagingBuffer{m_logicalDevice,
                         vertexSize,
                         m_vertexCount,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU};

    stagingBuffer.Map();
    stagingBuffer.WriteToBuffer((void*)vertices.data(), bufferSize);

    m_vertexBuffer =
        std::make_unique<Buffer>(m_logicalDevice, vertexSize, m_vertexCount,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    Buffer::CopyBuffer(m_logicalDevice, stagingBuffer, *m_vertexBuffer, bufferSize);
}

void Model::CreateIndexBuffer(const std::vector<u32>& indices)
{
    m_indexCount = static_cast<u32>(indices.size());
    VkDeviceSize bufferSize = indices.size() * sizeof(indices[0]);
    u32          indexSize = sizeof(indices[0]);

    Buffer stagingBuffer{m_logicalDevice,
                         indexSize,
                         m_indexCount,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU};
    stagingBuffer.Map();
    stagingBuffer.WriteToBuffer((void*)indices.data(), bufferSize);

    m_indexBuffer =
        std::make_unique<Buffer>(m_logicalDevice, indexSize, m_indexCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    Buffer::CopyBuffer(m_logicalDevice, stagingBuffer, *m_indexBuffer, bufferSize);
}

void Model::WriteDescriptorSet(DescriptorSetLayout& layout, DescriptorPool& pool, VkDescriptorSet descriptorSet)
{
    DescriptorWriter writer{layout, pool};
    auto             descriptorInfo = m_texture->GetDescriptorInfo();
    writer.WriteImage(0, &descriptorInfo);
    writer.Overwrite(descriptorSet);
}

void Model::Bind(VkCommandBuffer cmd)
{
    VkBuffer     buffers[] = {m_indexBuffer->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    // vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(cmd, buffers[0], 0, VK_INDEX_TYPE_UINT32);
}

void Model::Draw(VkCommandBuffer cmd) { vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0); }

std::vector<VkVertexInputBindingDescription> Vertex::GetBindingDescriptions()
{
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> Vertex::GetAttributeDescriptions()
{
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

    return attributeDescriptions;
}
} // namespace Humongous
