#include "allocator.hpp"

namespace Humongous
{
void Allocator::Initialize(LogicalDevice* logicalDevice)
{
    if(m_initialized) { return; }

    m_logicalDevice = logicalDevice;

    VkBufferCreateInfo bufInfo{};
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateInfo bufAlloc{};
    bufAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    u32               memTypeIndex;
    VmaPoolCreateInfo vertInfo{};
    vertInfo.memoryTypeIndex = vmaFindMemoryTypeIndexForBufferInfo(m_logicalDevice->GetVmaAllocator(), &bufInfo, &bufAlloc, &memTypeIndex);
    vertInfo.minAllocationAlignment = 1;

    vmaCreatePool(m_logicalDevice->GetVmaAllocator(), &vertInfo, &m_vertexBufPool);

    VkImageCreateInfo imgCI{};
    // imgCI.

    VmaPoolCreateInfo imgInfo{};

    m_initialized = true;
}

void Allocator::Shutdown()
{
    if(!m_initialized) { return; }

    vmaDestroyPool(m_logicalDevice->GetVmaAllocator(), m_vertexBufPool);
    // vmaDestroyPool(m_logicalDevice->GetVmaAllocator(), m_imagePool);

    m_initialized = false;
}

} // namespace Humongous
