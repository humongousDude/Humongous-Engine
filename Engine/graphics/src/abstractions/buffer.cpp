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

Buffer::Buffer(const BufferCreateInfo& createInfo)
    : m_logicalDevice{createInfo.device}, m_instanceSize{createInfo.size}, m_instanceCount{createInfo.instanceCount},
      m_usageFlags{createInfo.bufferUsage}, m_memoryPropertyFlags{createInfo.properties}
{
    Init(createInfo);
}

b8 IsValidMemoryPropertyCombination(vk::MemoryPropertyFlags memoryPropertyFlags)
{
    if((memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) && (memoryPropertyFlags & vk::MemoryPropertyFlagBits::eLazilyAllocated))
    {
        return false;
    }
    else if(((memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) ||
             (memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) ||
             (memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCached)) &&
            (memoryPropertyFlags & vk::MemoryPropertyFlagBits::eProtected))
    {
        return false;
    }

    return true;
}

b8 RequiresExplicitVMAAccesFlag(const VmaMemoryUsage& usage)
{
    if(usage & VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO) { return true; }
    if(usage & VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) { return true; }
    if(usage & VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO_PREFER_HOST) { return true; }
    return false;
}

void Buffer::Init(const BufferCreateInfo& createInfo)
{
    m_instanceSize = createInfo.size;
    m_instanceCount = createInfo.instanceCount;
    m_usageFlags = createInfo.bufferUsage;
    m_memoryPropertyFlags = createInfo.properties;
    m_alignmentSize = GetAlignment(m_instanceSize, createInfo.minOffsetAlignment);
    m_bufferSize = m_alignmentSize * m_instanceCount;
    m_sharingMode = createInfo.sharingMode;
    m_queueFamilyIndices = createInfo.queueFamilyIndices;
    m_name = createInfo.name;

    if(createInfo.size <= 0)
    {
        HGERROR("Cannot create buffer with <= 0 memory");
        return;
    }
    if(!IsValidMemoryPropertyCombination(createInfo.properties))
    {
        HGERROR("Cannot create buffer with invalid memory property combination");
        return;
    }

    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = m_instanceSize;
    bufferInfo.usage = m_usageFlags;
    bufferInfo.sharingMode = m_sharingMode;

    if(m_sharingMode != vk::SharingMode::eExclusive)
    {
        bufferInfo.queueFamilyIndexCount = static_cast<u32>(m_queueFamilyIndices.size());
        bufferInfo.pQueueFamilyIndices = m_queueFamilyIndices.data();
    }

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = createInfo.memoryUsage;
    allocCreateInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(m_memoryPropertyFlags);

    if(RequiresExplicitVMAAccesFlag(createInfo.memoryUsage)) { allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; }

    vk::Result result = m_logicalDevice.GetAllocator().AllocateBuffer(m_instanceSize, createInfo.memoryUsage, m_usageFlags, allocCreateInfo,
                                                                      m_memoryPropertyFlags, m_allocation, m_buffer);

    if(result != vk::Result::eSuccess)
    {
        HGERROR("Failed to create buffer: %s", vk::to_string(result).c_str());
        return;
    }

    m_logicalDevice.GetAllocator().NameAllocation(m_allocation, createInfo.name.c_str());

    VmaAllocationInfo allocInfo = {};
    allocInfo = m_logicalDevice.GetAllocator().GetAllocationInfo(m_allocation);

    if(!m_allocation)
    {
        HGERROR("Failed to get allocation info");
        return;
    }

    m_allocationInfo = allocInfo;
    m_isValid = true;

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

/**
 * Map a memory range of this buffer. If successful, m_mapped points to the specified m_buffer range.
 *
 * @param size (Optional) Size of the memory range to map. Pass vk::WholeSize to map the complete
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
    if(!(m_memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostVisible))
    {
        HGERROR("Cannot map a buffer that is not host visible");
        return vk::Result::eErrorOutOfDeviceMemory;
    }

    m_isMapped = true;

    if(!m_allocationInfo.pMappedData) { return m_logicalDevice.GetAllocator().Map(m_allocation, &m_allocationInfo.pMappedData); }
    else
    {
        return vk::Result::eSuccess;
    }
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
    if(!m_isMapped)
    {
        HGWARN("Trying to unmap a buffer that is not mapped");
        return;
    }

    if(m_allocationInfo.pMappedData)
    {
        if(!(m_memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent)) { Invalidate(); }

        if(m_allocationInfo.pMappedData) { m_logicalDevice.GetAllocator().Unmap(m_allocation); }

        m_allocationInfo.pMappedData = nullptr;
    }

    m_isMapped = false;
}

/**
 * Copies the specified data to the mapped buffer. Default value writes whole m_buffer range
 *
 * @param data Pointer to the data to copy
 * @param size (Optional) Size of the data to copy. Pass vk::WholeSize to flush the complete m_buffer
 * range.
 * @param offset (Optional) Byte offset from beginning of m_mapped region
 *
 */
void Buffer::WriteToBuffer(void* data, vk::DeviceSize size, vk::DeviceSize offset)
{
    if(!m_isValid)
    {
        HGERROR("Cannot write to a null buffer. It's likely that an internal function failed.");
        return;
    }
    if(!m_isMapped)
    {
        HGERROR("Cannot copy to unmapped buffer");
        return;
    }
    if(!data)
    {
        HGERROR("Cannot write invalid data to buffer");
        return;
    }

    if(size == vk::WholeSize) { size = m_bufferSize; }

    if(size <= 0)
    {
        HGERROR("Cannot write <= 0 bytes to buffer");
        return;
    }

    if(offset < 0)
    {
        HGERROR("Cannot write to buffer with negative offset");
        return;
    }

    if(offset > m_bufferSize || offset + size > m_bufferSize)
    {
        HGERROR("Write exceeds \"%s\"'s bounds (Buffer: %i bytes, Offset: %i bytes, Size: %i bytes)", m_name.c_str(), m_bufferSize, offset, size);
        return;
    }

    char* memOffset = static_cast<char*>(m_allocationInfo.pMappedData) + offset;
    memcpy(memOffset, data, size);
}

/**
 * Flush a memory range of the buffer to make it visible to the device
 *
 * @note Only required for non-coherent m_memory
 *
 * @param size (Optional) Size of the m_memory range to flush. Pass vk::WholeSize to flush the
 * complete m_buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return vk::Result of the flush call
 */
vk::Result Buffer::Flush(vk::DeviceSize size, vk::DeviceSize offset)
{
    if(!m_isValid)
    {
        HGERROR("Cannot flush a null buffer. It's likely that an internal function failed.");
        return vk::Result::eErrorOutOfDeviceMemory;
    }
    if(!m_isMapped)
    {
        HGERROR("Cannot flush an unmapped buffer");
        return vk::Result::eErrorOutOfDeviceMemory;
    }
    if(!(m_memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostVisible))
    {
        HGERROR("Cannot flush a buffer that is not host visible");
        return vk::Result::eErrorOutOfDeviceMemory;
    }

    if(size == vk::WholeSize) { size = m_bufferSize; }

    if(offset > m_bufferSize || offset + size > m_bufferSize)
    {
        HGERROR("Write exceeds \"%s\"'s bounds (Buffer: %i bytes, Offset: %i bytes, Size: %i bytes)", m_name.c_str(), m_bufferSize, offset, size);
        return vk::Result::eErrorOutOfDeviceMemory;
    }

    vk::MappedMemoryRange mappedRange = {};
    mappedRange.sType = vk::StructureType::eMappedMemoryRange;
    mappedRange.memory = m_allocationInfo.deviceMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return m_logicalDevice.FlushMappedMemoryRanges({mappedRange});
}

/**
 * Invalidate a memory range of the buffer to make it visible to the host
 *
 * @note Only required for non-coherent memory
 *
 * @param size (Optional) Size of the memory range to invalidate. Pass vk::WholeSize to invalidate
 * the complete m_buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return vk::Result of the invalidate call
 */
vk::Result Buffer::Invalidate(vk::DeviceSize size, vk::DeviceSize offset)
{
    if(!m_isValid)
    {
        HGERROR("Cannot invalidate a null buffer. It's likely that an internal function failed.");
        return vk::Result::eErrorOutOfDeviceMemory;
    }
    if(!m_isMapped)
    {
        HGERROR("Cannot invalidate an unmapped buffer");
        return vk::Result::eErrorOutOfDeviceMemory;
    }
    if(!(m_memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostVisible))
    {
        HGERROR("Cannot invalidate a buffer that is not host visible");
        return vk::Result::eErrorOutOfDeviceMemory;
    }
    if(size == vk::WholeSize) { size = m_bufferSize; }
    if(offset > m_bufferSize || offset + size > m_bufferSize)
    {
        HGERROR("Invalidate exceeds \"%s\"'s bounds (Buffer: %i bytes, Offset: %i bytes, Size: %i bytes)", m_name.c_str(), m_bufferSize, offset,
                size);
        return vk::Result::eErrorOutOfDeviceMemory;
    }

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

void Buffer::CopyBuffer(const ILogicalDevice& device, vk::CommandBuffer cmd, Buffer& srcBuffer, Buffer& dstBuffer, vk::DeviceSize size)
{
    if(!srcBuffer.IsValid() || !dstBuffer.IsValid())
    {
        HGERROR("Trying to copy a buffer that has not been created");
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

    device.RecordCopyBuffer(cmd, copyBufferInfo);

    srcBuffer.UpdateAddress(srcBuffer.GetUsageFlags());
    dstBuffer.UpdateAddress(dstBuffer.GetUsageFlags());
}

void Buffer::TransferQueue(const vk::CommandBuffer& cmd, const u32& srcQueueFamilyIndex, const u32& dstQueueFamilyIndex,
                           const vk::PipelineStageFlags2& srcStage, const vk::PipelineStageFlags2& dstStage, const vk::AccessFlags2& srcAccess,
                           const vk::AccessFlags2& dstAccess)
{
    // A barrier is only needed if the source and destination queue families are different.
    if(srcQueueFamilyIndex == dstQueueFamilyIndex) { return; }

    vk::BufferMemoryBarrier2 bufferBarrier{};
    bufferBarrier.srcStageMask = srcStage;
    bufferBarrier.srcAccessMask = srcAccess;
    bufferBarrier.dstStageMask = dstStage;
    bufferBarrier.dstAccessMask = dstAccess;
    bufferBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    bufferBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    bufferBarrier.buffer = m_buffer;
    bufferBarrier.offset = 0;
    bufferBarrier.size = m_bufferSize;

    vk::DependencyInfo dependencyInfo{};
    dependencyInfo.pNext = nullptr;
    dependencyInfo.bufferMemoryBarrierCount = 1;
    dependencyInfo.pBufferMemoryBarriers = &bufferBarrier;

    cmd.pipelineBarrier2(dependencyInfo);
}

void Buffer::CopyToImage(vk::CommandBuffer cmd, Image& image, const std::vector<vk::BufferImageCopy>& regions)
{
    if(m_buffer == VK_NULL_HANDLE || !image.IsValid())
    {
        HGERROR("Unable to copy buffer to image, buffer or image is null");
        return;
    }

    m_logicalDevice.RecordCopyBufferToImage(cmd, m_buffer, image.GetImage(), vk::ImageLayout::eTransferDstOptimal, regions);
}

void Buffer::CopyToImage(vk::CommandBuffer cmd, Image& image, const u32& width, const u32& height)
{
    if(m_buffer == VK_NULL_HANDLE || !image.IsValid())
    {
        HGERROR("Unable to copy buffer to image, buffer or image is null");
        return;
    }

    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D(0, 0, 0);
    region.imageExtent = vk::Extent3D(width, height, 1);

    m_logicalDevice.RecordCopyBufferToImage(cmd, m_buffer, image.GetImage(), vk::ImageLayout::eTransferDstOptimal, {region});
}

} // namespace Humongous
