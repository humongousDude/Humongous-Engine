#include "allocator.hpp"

namespace Humongous
{
void Allocator::Internal_Initialize(LogicalDevice* logicalDevice)
{
    if(m_initialized) { return; }

    m_logicalDevice = logicalDevice;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateInfo bufAlloc{};
    bufAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    n32               memTypeIndex;
    VmaPoolCreateInfo vertInfo{};
    vertInfo.memoryTypeIndex = vmaFindMemoryTypeIndexForBufferInfo(m_logicalDevice->GetVmaAllocator(), &bufInfo, &bufAlloc, &memTypeIndex);
    vertInfo.minAllocationAlignment = 1;

    vmaCreatePool(m_logicalDevice->GetVmaAllocator(), &vertInfo, &m_vertexBufPool);

    VkImageCreateInfo gltfCI{};
    gltfCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    gltfCI.imageType = VK_IMAGE_TYPE_2D;
    gltfCI.format = VK_FORMAT_R8G8B8A8_UNORM;
    gltfCI.extent.width = 1024;
    gltfCI.extent.height = 1024;
    gltfCI.extent.depth = 1;
    gltfCI.mipLevels = 11;
    gltfCI.arrayLayers = 1;
    gltfCI.samples = VK_SAMPLE_COUNT_1_BIT;
    gltfCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    gltfCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaPoolCreateInfo gltfPoolCI{};
    gltfPoolCI.memoryTypeIndex = vmaFindMemoryTypeIndexForImageInfo(m_logicalDevice->GetVmaAllocator(), &gltfCI, &bufAlloc, &memTypeIndex);
    gltfPoolCI.minAllocationAlignment = 1;

    vmaCreatePool(m_logicalDevice->GetVmaAllocator(), &gltfPoolCI, &m_gltfImgPool);

    // TODO: create a pool for all the rest

    m_initialized = true;
}

void Allocator::Internal_Shutdown()
{
    if(!m_initialized) { return; }

    vmaDestroyPool(m_logicalDevice->GetVmaAllocator(), m_vertexBufPool);
    vmaDestroyPool(m_logicalDevice->GetVmaAllocator(), m_gltfImgPool);

    m_initialized = false;
}

} // namespace Humongous
