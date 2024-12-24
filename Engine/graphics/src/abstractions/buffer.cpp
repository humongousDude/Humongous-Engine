// Original from Brendan Galea's vulkan tutorial, adapted to use VMA

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

// TODO: Change this to use vulkan.hpp

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

Buffer::Buffer(LogicalDevice* device, VkDeviceSize instanceSize, n32 instanceCount, VkBufferUsageFlags usageFlags,
               VkMemoryPropertyFlags memoryPropertyFlags, VmaMemoryUsage memoryUsage, VkDeviceSize minOffsetAlignment)
    : m_logicalDevice{device}, m_instanceSize{instanceSize}, m_instanceCount{instanceCount}, m_usageFlags{usageFlags},
      m_memoryPropertyFlags{memoryPropertyFlags}
{
    Init(device, instanceSize, instanceCount, usageFlags, memoryPropertyFlags, memoryUsage, minOffsetAlignment);
}

Buffer::Buffer() : m_logicalDevice{nullptr}, m_instanceSize{0}, m_instanceCount{0}, m_usageFlags{0}, m_memoryPropertyFlags{0} {}

void Buffer::Init(LogicalDevice* device, VkDeviceSize instanceSize, n32 instanceCount, VkBufferUsageFlags usageFlags,
                  VkMemoryPropertyFlags memoryPropertyFlags, VmaMemoryUsage memoryUsage, VkDeviceSize minOffsetAlignment)
{
    m_logicalDevice = device;
    m_instanceSize = instanceSize;
    m_instanceCount = instanceCount;
    m_usageFlags = usageFlags;
    m_memoryPropertyFlags = memoryPropertyFlags;

    m_alignmentSize = GetAlignment(m_instanceSize, minOffsetAlignment);
    m_bufferSize = m_alignmentSize * m_instanceCount;

    CreateInfo createInfo{
        .device = m_logicalDevice,
        .size = m_bufferSize,
        .bufferUsage = m_usageFlags,
        .properties = m_memoryPropertyFlags,
        .buffer = &m_buffer,
        .memory = m_allocationInfo.deviceMemory,
        .allocation = m_allocation,
        .minOffsetAlignment = minOffsetAlignment,
    };

    CreateBuffer(createInfo);

    UpdateAddress(m_usageFlags);
}

Buffer::~Buffer()
{
    if(m_allocationInfo.pMappedData) { UnMap(); }
    if(m_buffer != VK_NULL_HANDLE) { vmaDestroyBuffer(m_logicalDevice->GetVmaAllocator(), m_buffer, m_allocation); }
}

void Buffer::CreateBuffer(CreateInfo& createInfo)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = createInfo.size;
    bufferInfo.usage = createInfo.bufferUsage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = createInfo.memoryUsage;
    allocCreateInfo.requiredFlags = createInfo.properties;

    VmaAllocationInfo allocInfo{};
    allocInfo.offset = 0;
    allocInfo.size = createInfo.size;
    allocInfo.deviceMemory = createInfo.memory;
    allocInfo.pMappedData = nullptr;
    allocInfo.pUserData = nullptr;

    vmaCreateBufferWithAlignment(createInfo.device->GetVmaAllocator(), &bufferInfo, &allocCreateInfo, createInfo.minOffsetAlignment,
                                 createInfo.buffer, &createInfo.allocation, &allocInfo);

    VmaAllocationInfo ret{};
    vmaGetAllocationInfo(createInfo.device->GetVmaAllocator(), createInfo.allocation, &ret);
    m_allocationInfo = ret;
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
    HGASSERT(m_buffer && m_allocationInfo.deviceMemory && "Called map on buffer before create");

    m_mapCallCount++;
    return vmaMapMemory(m_logicalDevice->GetVmaAllocator(), m_allocation, &m_allocationInfo.pMappedData);
}

/**
 * Unmap a mapped memory range
 *
 * @note Does not return a result as vkUnmapMemory can't fail
 */
void Buffer::UnMap()
{
    if(m_allocationInfo.pMappedData)
    {
        if(!(m_memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { Invalidate(); }

        for(int i = 0; i < m_mapCallCount; i++) { vmaUnmapMemory(m_logicalDevice->GetVmaAllocator(), m_allocation); }

        m_allocationInfo.pMappedData = nullptr;
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
    assert(m_allocationInfo.pMappedData && "Cannot copy to unmapped buffer");

    if(size == VK_WHOLE_SIZE) { memcpy(m_allocationInfo.pMappedData, data, m_bufferSize); }
    else
    {
        char* memOffset = (char*)m_allocationInfo.pMappedData;
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
    mappedRange.memory = m_allocationInfo.deviceMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkFlushMappedMemoryRanges(m_logicalDevice->GetVkDevice(), 1, &mappedRange);
}

/**
 * Invalidate a memory range of the buffer to make it visible to the host
 *
 * @note Only required for non-coherent memory
 *
 * @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate
 * the complete m_buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the invalidate call
 */
VkResult Buffer::Invalidate(VkDeviceSize size, VkDeviceSize offset)
{
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = m_allocationInfo.deviceMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;

    return vmaInvalidateAllocation(m_logicalDevice->GetVmaAllocator(), m_allocation, offset, size);
}

void Buffer::UpdateAddress(VkBufferUsageFlags usage)
{

    if(!(usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)) { return; }
    VkBufferDeviceAddressInfo bufferDeviceAddressInfo{};
    bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAddressInfo.buffer = m_buffer;

    m_deviceAddress = vkGetBufferDeviceAddress(m_logicalDevice->GetVkDevice(), &bufferDeviceAddressInfo);
}

/**
 * Create a buffer info descriptor
 *
 * @param size (Optional) Size of the m_memory range of the descriptor
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkDescriptorBufferInfo of specified offset and range
 */
VkDescriptorBufferInfo Buffer::DescriptorInfo(VkDeviceSize size, VkDeviceSize offset) const
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

void Buffer::CopyBuffer(LogicalDevice& device, Buffer& srcBuffer, Buffer& dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = device.BeginSingleTimeCommands();

    VkBufferCopy2 copyRegion{};
    copyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    copyRegion.pNext = nullptr;

    VkCopyBufferInfo2 copyBufferInfo{};
    copyBufferInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
    copyBufferInfo.srcBuffer = srcBuffer.GetBuffer();
    copyBufferInfo.dstBuffer = dstBuffer.GetBuffer();
    copyBufferInfo.regionCount = 1;
    copyBufferInfo.pRegions = &copyRegion;
    copyBufferInfo.pNext = nullptr;

    vkCmdCopyBuffer2(commandBuffer, &copyBufferInfo);

    device.EndSingleTimeCommands(commandBuffer);

    srcBuffer.UpdateAddress(srcBuffer.GetUsageFlags());
    dstBuffer.UpdateAddress(dstBuffer.GetUsageFlags());
}
} // namespace Humongous
