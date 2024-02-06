/*
 * Encapsulates a vulkan buffer
 *
 * Initially based off VulkanBuffer by Sascha Willems -
 * https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanBuffer.h
 */

#include <abstractions/buffer.hpp>

// std
#include <cassert>
#include <cstring>

namespace Humongous
{

/**
 * Returns the minimum instance size required to be compatible with devices minOffsetAlignment
 *
 * @param m_instanceSize The size of an instance
 * @param minOffsetAlignment The minimum required alignment, in bytes, for the offset member (eg
 * minUniformBufferOffsetAlignment)
 *
 * @return VkResult of the buffer mapping call
 */
VkDeviceSize Buffer::GetAlignment(VkDeviceSize m_instanceSize, VkDeviceSize minOffsetAlignment)
{
    if(minOffsetAlignment > 0) { return (m_instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1); }
    return m_instanceSize;
}

Buffer::Buffer(LogicalDevice& device, VkDeviceSize m_instanceSize, uint32_t m_instanceCount, VkBufferUsageFlags usageFlags,
               VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize minOffsetAlignment)
    : m_logicalDevice{device}, m_instanceSize{m_instanceSize}, m_instanceCount{m_instanceCount}, m_usageFlags{usageFlags},
      m_memoryPropertyFlags{memoryPropertyFlags}
{
    m_alignmentSize = GetAlignment(m_instanceSize, minOffsetAlignment);
    m_bufferSize = m_alignmentSize * m_instanceCount;

    CreateInfo createInfo{.device = m_logicalDevice,
                          .size = m_bufferSize,
                          .usage = m_usageFlags,
                          .properties = m_memoryPropertyFlags,
                          .buffer = &m_buffer,
                          .memory = m_memory,
                          .allocation = m_allocation};

    CreateBuffer(createInfo);
}

Buffer::~Buffer()
{
    UnMap();
    vmaDestroyBuffer(m_logicalDevice.GetVmaAllocator(), m_buffer, m_allocation);
}

void Buffer::CreateBuffer(CreateInfo& createInfo)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = createInfo.size;
    bufferInfo.usage = createInfo.usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocCreateInfo.requiredFlags = createInfo.properties;

    VmaAllocationInfo allocInfo{};
    allocInfo.pMappedData = createInfo.data;
    allocInfo.offset = 0;
    allocInfo.deviceMemory = createInfo.memory;
    allocInfo.pMappedData = createInfo.data;

    vmaCreateBuffer(createInfo.device.GetVmaAllocator(), &bufferInfo, &allocCreateInfo, createInfo.buffer, &createInfo.allocation, &allocInfo);

    VmaAllocationInfo returned{};
    vmaGetAllocationInfo(createInfo.device.GetVmaAllocator(), createInfo.allocation, &returned);
    m_memory = returned.deviceMemory;
    m_mapped = returned.pMappedData;
}

/**
 * Map a memory range of this buffer. If successful, m_mapped points to the specified m_buffer range.
 *
 * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete
 * buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the buffer mapping call
 */
VkResult Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
{
    assert(m_buffer && m_memory && "Called map on m_buffer before create");
    return vkMapMemory(m_logicalDevice.GetVkDevice(), m_memory, offset, size, 0, &m_mapped);
}

/**
 * Unmap a m_mapped m_memory range
 *
 * @note Does not return a result as vkUnmapMemory can't fail
 */
void Buffer::UnMap()
{
    if(m_mapped)
    {
        vkUnmapMemory(m_logicalDevice.GetVkDevice(), m_memory);
        m_mapped = nullptr;
    }
}

/**
 * Copies the specified data to the m_mapped m_buffer. Default value writes whole m_buffer range
 *
 * @param data Pointer to the data to copy
 * @param size (Optional) Size of the data to copy. Pass VK_WHOLE_SIZE to flush the complete m_buffer
 * range.
 * @param offset (Optional) Byte offset from beginning of m_mapped region
 *
 */
void Buffer::WriteToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset)
{
    assert(m_mapped && "Cannot copy to unmapped m_buffer");

    if(size == VK_WHOLE_SIZE) { memcpy(m_mapped, data, m_bufferSize); }
    else
    {
        char* memOffset = (char*)m_mapped;
        memOffset += offset;
        memcpy(memOffset, data, size);
    }
}

/**
 * Flush a m_memory range of the m_buffer to make it visible to the device
 *
 * @note Only required for non-coherent m_memory
 *
 * @param size (Optional) Size of the m_memory range to flush. Pass VK_WHOLE_SIZE to flush the
 * complete m_buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the flush call
 */
VkResult Buffer::Flush(VkDeviceSize size, VkDeviceSize offset)
{
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = m_memory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkFlushMappedMemoryRanges(m_logicalDevice.GetVkDevice(), 1, &mappedRange);
}

/**
 * Invalidate a m_memory range of the m_buffer to make it visible to the host
 *
 * @note Only required for non-coherent m_memory
 *
 * @param size (Optional) Size of the m_memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate
 * the complete m_buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the invalidate call
 */
VkResult Buffer::Invalidate(VkDeviceSize size, VkDeviceSize offset)
{
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = m_memory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkInvalidateMappedMemoryRanges(m_logicalDevice.GetVkDevice(), 1, &mappedRange);
}

/**
 * Create a m_buffer info descriptor
 *
 * @param size (Optional) Size of the m_memory range of the descriptor
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkDescriptorBufferInfo of specified offset and range
 */
VkDescriptorBufferInfo Buffer::DescriptorInfo(VkDeviceSize size, VkDeviceSize offset)
{
    return VkDescriptorBufferInfo{
        m_buffer,
        offset,
        size,
    };
}

/**
 * Copies "m_instanceSize" bytes of data to the m_mapped m_buffer at an offset of index * m_alignmentSize
 *
 * @param data Pointer to the data to copy
 * @param index Used in offset calculation
 *
 */
void Buffer::WriteToIndex(void* data, int index) { WriteToBuffer(data, m_instanceSize, index * m_alignmentSize); }

/**
 *  Flush the m_memory range at index * m_alignmentSize of the m_buffer to make it visible to the device
 *
 * @param index Used in offset calculation
 *
 */
VkResult Buffer::FlushIndex(int index) { return Flush(m_alignmentSize, index * m_alignmentSize); }

/**
 * Create a m_buffer info descriptor
 *
 * @param index Specifies the region given by index * m_alignmentSize
 *
 * @return VkDescriptorBufferInfo for instance at index
 */
VkDescriptorBufferInfo Buffer::DescriptorInfoForIndex(int index) { return DescriptorInfo(m_alignmentSize, index * m_alignmentSize); }

/**
 * Invalidate a m_memory range of the m_buffer to make it visible to the host
 *
 * @note Only required for non-coherent m_memory
 *
 * @param index Specifies the region to invalidate: index * m_alignmentSize
 *
 * @return VkResult of the invalidate call
 */
VkResult Buffer::InvalidateIndex(int index) { return Invalidate(m_alignmentSize, index * m_alignmentSize); }

} // namespace Humongous
