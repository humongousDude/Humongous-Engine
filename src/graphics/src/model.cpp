#include <model.hpp>

namespace Humongous
{
Model::Model(LogicalDevice& device, const std::vector<Vertex>& vertices) : m_logicalDevice{device} { CreateVertexBuffer(vertices); }

Model::~Model() {}

void Model::CreateVertexBuffer(const std::vector<Vertex>& vertices)
{
    m_vertexCount = static_cast<u32>(vertices.size());
    HGASSERT(m_vertexCount >= 3);

    VkDeviceSize bufferSize = m_vertexCount * sizeof(Vertex);
    u32          vertexSize = sizeof(Vertex);

    Buffer stagingBuffer{m_logicalDevice,
                         vertexSize,
                         m_vertexCount,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU};
    stagingBuffer.Map();
    stagingBuffer.WriteToBuffer((void*)vertices.data());

    m_vertexBuffer =
        std::make_unique<Buffer>(m_logicalDevice, vertexSize, m_vertexCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    Buffer::CopyBuffer(m_logicalDevice, stagingBuffer.GetBuffer(), m_vertexBuffer->GetBuffer(), bufferSize);
}

void Model::Bind(VkCommandBuffer cmd)
{
    VkBuffer     buffers[] = {m_vertexBuffer->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
}

void Model::Draw(VkCommandBuffer cmd) { vkCmdDraw(cmd, m_vertexCount, 1, 0, 0); }

std::vector<VkVertexInputBindingDescription> Vertex::GetBindingDescriptions()
{
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(Vertex);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> Vertex::GetAttributeDescriptions()
{
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    return attributeDescriptions;
}
} // namespace Humongous
