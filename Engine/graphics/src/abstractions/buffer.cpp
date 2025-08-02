// Original from Brendan Galea's vulkan tutorial, adapted to use VMA

/*
 * Encapsulates a vulkan buffer
 *
 * Initially based off VulkanBuffer by Sascha Willems -
 * https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanBuffer.h
 */

#include "logger.hpp"
#include <abstractions/buffer.hpp>

// std
#include <cstring>

namespace Humongous
{

Buffer::Buffer(const ILogicalDevice& device, vk::DeviceSize instanceSize, u32 instanceCount, vk::BufferUsageFlags usageFlags,
               vk::MemoryPropertyFlags memoryPropertyFlags, VmaMemoryUsage memoryUsage, vk::DeviceSize minOffsetAlignment, const std::string& name)
    : m_logicalDevice{device}, m_instanceSize{instanceSize}, m_instanceCount{instanceCount}, m_usageFlags{usageFlags},
      m_memoryPropertyFlags{memoryPropertyFlags}
{
    Init(instanceSize, instanceCount, usageFlags, memoryPropertyFlags, memoryUsage, minOffsetAlignment, name);
}

Buffer::Buffer(const BufferCreateInfo& createInfo)
    : m_logicalDevice{createInfo.device}, m_instanceSize{createInfo.size}, m_instanceCount{createInfo.instanceCount},
      m_usageFlags{createInfo.bufferUsage}, m_memoryPropertyFlags{createInfo.properties}
{
    Init(createInfo.size, 1, createInfo.bufferUsage, createInfo.properties, createInfo.memoryUsage, createInfo.minOffsetAlignment, createInfo.name);
}

void Buffer::Init(vk::DeviceSize instanceSize, u32 instanceCount, vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memoryPropertyFlags,
                  VmaMemoryUsage memoryUsage, vk::DeviceSize minOffsetAlignment, const std::string& name)
{
    m_instanceSize = instanceSize;
    m_instanceCount = instanceCount;
    m_usageFlags = usageFlags;
    m_memoryPropertyFlags = memoryPropertyFlags;
    m_alignmentSize = GetAlignment(m_instanceSize, minOffsetAlignment);
    m_bufferSize = m_alignmentSize * m_instanceCount;
    m_name = name;

    CreateInfo createInfo{.device = m_logicalDevice,
                          .size = m_bufferSize,
                          .bufferUsage = m_usageFlags,
                          .properties = m_memoryPropertyFlags,
                          .buffer = &m_buffer,
                          .memory = m_allocationInfo.deviceMemory,
                          .allocation = m_allocation,
                          .minOffsetAlignment = minOffsetAlignment,
                          .name = m_name};

    CreateBuffer(createInfo);

    UpdateAddress(m_usageFlags);
}

Buffer::~Buffer()
{
    if(m_allocationInfo.pMappedData) { UnMap(); }
    if(m_isValid) { m_logicalDevice.GetAllocator().FreeBuffer(m_allocation, m_buffer); }
}

/**
 * Returns the minimum instance size required to be compatible with devices minOffsetAlignment
 *
 * @param m_instanceSize The size of an instance
 * @param minOffsetAlignment The minimum required alignment, in bytes, for the offset member (eg
 * minUniformBufferOffsetAlignment)
 *
 * @return vk::Result of the buffer mapping call
 */
vk::DeviceSize Buffer::GetAlignment(vk::DeviceSize m_instanceSize, vk::DeviceSize minOffsetAlignment)
{
    if(minOffsetAlignment > 0) { return (m_instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1); }
    return m_instanceSize;
}

void Buffer::CreateBuffer(CreateInfo& createInfo)
{
    if(createInfo.size <= 0)
    {
        HGERROR("Cannot create buffer with <= 0 memory");
        return;
    }

    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = createInfo.size;
    bufferInfo.usage = createInfo.bufferUsage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = createInfo.memoryUsage;
    allocCreateInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(createInfo.properties);

    vk::Result result = m_logicalDevice.GetAllocator().AllocateBuffer(createInfo.size, createInfo.memoryUsage, createInfo.bufferUsage,
                                                                      createInfo.properties, createInfo.allocation, *createInfo.buffer);

    if(result != vk::Result::eSuccess)
    {
        HGERROR("Failed to create buffer: %s", vk::to_string(result).c_str());
        return;
    }

    m_logicalDevice.GetAllocator().NameAllocation(createInfo.allocation, createInfo.name.c_str());

    VmaAllocationInfo allocInfo = {};
    allocInfo = m_logicalDevice.GetAllocator().GetAllocationInfo(createInfo.allocation);

    if(!createInfo.allocation)
    {
        HGERROR("Failed to get allocation info");
        return;
    }

    m_allocationInfo = allocInfo;
    m_isValid = true;
}

/**
 * Map a memory range of this buffer. If successful, m_mapped points to the specified m_buffer range.
 *
 * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete
 * buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return vk::Result of the buffer mapping call
 */
vk::Result Buffer::Map(vk::DeviceSize size, vk::DeviceSize offset)
{
    if(!m_isValid)
    {
        HGERROR("Called map on buffer before create");
        // what's the correct error here?
        return vk::Result::eErrorUnknown;
    }

    if(!m_allocationInfo.pMappedData) { return m_logicalDevice.GetAllocator().Map(m_allocation, &m_allocationInfo.pMappedData); }
    else { return vk::Result::eSuccess; }
}

/**
 * Unmap a mapped memory range
 *
 * @note Does not return a result as vkUnmapMemory can't fail
 */
void Buffer::UnMap()
{
    if(!m_isValid)
    {
        HGERROR("Trying to unmap a buffer that has not been created");
        return;
    }

    if(m_allocationInfo.pMappedData)
    {
        if(!(m_memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent)) { Invalidate(); }

        if(m_allocationInfo.pMappedData) { m_logicalDevice.GetAllocator().Unmap(m_allocation); }

        m_allocationInfo.pMappedData = nullptr;
    }
}

/**
 * Copies the specified data to the mapped buffer. Default value writes whole m_buffer range
 *
 * @param data Pointer to the data to copy
 * @param size (Optional) Size of the data to copy. Pass VK_WHOLE_SIZE to flush the complete m_buffer
 * range.
 * @param offset (Optional) Byte offset from beginning of m_mapped region
 *
 */
void Buffer::WriteToBuffer(void* data, vk::DeviceSize size, vk::DeviceSize offset)
{
    if(!m_allocationInfo.pMappedData)
    {
        HGERROR("Cannot copy to unmapped buffer");
        return;
    }
    if(!data)
    {
        HGERROR("Cannot write invalid data to buffer");
        return;
    }
    if(!m_isValid)
    {
        HGERROR("Cannot write to a null buffer. It's likely that an internal function failed.");
        return;
    }
    if(offset + size > m_bufferSize && size != VK_WHOLE_SIZE)
    {
        HGERROR("Write exceeds \"%s\"'s bounds. Buffer is %i bytes, write is %i bytes", m_name.c_str(), m_bufferSize, size);
        return;
    }

    if(size == VK_WHOLE_SIZE) { size = m_bufferSize; }

    char* memOffset = static_cast<char*>(m_allocationInfo.pMappedData) + offset;
    memcpy(memOffset, data, size);
}

/**
 * Flush a memory range of the buffer to make it visible to the device
 *
 * @note Only required for non-coherent m_memory
 *
 * @param size (Optional) Size of the m_memory range to flush. Pass VK_WHOLE_SIZE to flush the
 * complete m_buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return vk::Result of the flush call
 */
vk::Result Buffer::Flush(vk::DeviceSize size, vk::DeviceSize offset)
{
    vk::MappedMemoryRange mappedRange = {};
    mappedRange.sType = vk::StructureType::eMappedMemoryRange;
    mappedRange.memory = m_allocationInfo.deviceMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return m_logicalDevice.GetVkDevice().flushMappedMemoryRanges(1, &mappedRange);
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
 * @return vk::Result of the invalidate call
 */
vk::Result Buffer::Invalidate(vk::DeviceSize size, vk::DeviceSize offset)
{
    vk::MappedMemoryRange mappedRange = {};
    mappedRange.sType = vk::StructureType::eMappedMemoryRange;
    mappedRange.memory = m_allocationInfo.deviceMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;

    return m_logicalDevice.GetAllocator().Invalidate(m_allocation, offset, size);
}

void Buffer::UpdateAddress(vk::BufferUsageFlags usage)
{
    if(!(usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)) { return; }
    vk::BufferDeviceAddressInfo bufferDeviceAddressInfo{};
    bufferDeviceAddressInfo.buffer = m_buffer;

    m_deviceAddress = m_logicalDevice.GetDeviceAddress(bufferDeviceAddressInfo);
}

/**
 * Create a buffer info descriptor
 *
 * @param size (Optional) Size of the m_memory range of the descriptor
 * @param offset (Optional) Byte offset from beginning
 *
 * @return vk::DescriptorBufferInfo of specified offset and range
 */
vk::DescriptorBufferInfo Buffer::DescriptorInfo(vk::DeviceSize size, vk::DeviceSize offset) const
{
    if(!m_isValid)
    {
        HGERROR("Cannot create descriptor info for a null buffer. It's likely that a previous function failed.");
        return vk::DescriptorBufferInfo{};
    }
    return vk::DescriptorBufferInfo{
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
vk::Result Buffer::FlushIndex(int index) { return Flush(m_alignmentSize, index * m_alignmentSize); }

/**
 * Create a m_buffer info descriptor
 *
 * @param index Specifies the region given by index * m_alignmentSize
 *
 * @return vk::DescriptorBufferInfo for instance at index
 */
vk::DescriptorBufferInfo Buffer::DescriptorInfoForIndex(int index) { return DescriptorInfo(m_alignmentSize, index * m_alignmentSize); }

/**
 * Invalidate a m_memory range of the m_buffer to make it visible to the host
 *
 * @note Only required for non-coherent m_memory
 *
 * @param index Specifies the region to invalidate: index * m_alignmentSize
 *
 * @return vk::Result of the invalidate call
 */
vk::Result Buffer::InvalidateIndex(int index) { return Invalidate(m_alignmentSize, index * m_alignmentSize); }

void Buffer::CopyBuffer(const ILogicalDevice& device, Buffer& srcBuffer, Buffer& dstBuffer, vk::DeviceSize size)
{
    if(!srcBuffer.IsValid() || !dstBuffer.IsValid())
    {
        HGERROR("Trying to copy a buffer that has not been created");
        return;
    }

    vk::CommandBuffer commandBuffer = device.BeginSingleTimeCommands();
    if(commandBuffer == VK_NULL_HANDLE)
    {
        HGERROR("Device failed to provide one-time submite command buffer");
        return;
    }
    if(size > dstBuffer.GetBufferSize())
    {
        HGERROR("Copy size is larger than destination buffer, need %i more bytes", size - dstBuffer.GetBufferSize());
        return;
    }

    vk::BufferCopy2 copyRegion{};
    copyRegion.sType = vk::StructureType::eBufferCopy2;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    copyRegion.pNext = nullptr;

    vk::CopyBufferInfo2 copyBufferInfo{};
    copyBufferInfo.sType = vk::StructureType::eCopyBufferInfo2;
    copyBufferInfo.srcBuffer = srcBuffer.GetBuffer();
    copyBufferInfo.dstBuffer = dstBuffer.GetBuffer();
    copyBufferInfo.regionCount = 1;
    copyBufferInfo.pRegions = &copyRegion;
    copyBufferInfo.pNext = nullptr;

    device.RecordCopyBuffer(commandBuffer, copyBufferInfo);

    device.EndSingleTimeCommands(commandBuffer);

    srcBuffer.UpdateAddress(srcBuffer.GetUsageFlags());
    dstBuffer.UpdateAddress(dstBuffer.GetUsageFlags());
}

} // namespace Humongous
