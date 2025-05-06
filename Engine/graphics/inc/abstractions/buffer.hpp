// Original from Brendan Galea's vulkan tutorial, adapted to use VMA
#pragma once

#include "non_copyable.hpp"
#include <glm/fwd.hpp>
#include <logical_device.hpp>
#include <vk_mem_alloc.h>

// TODO: Change this to use vulkan.hpp

namespace Humongous
{
class Buffer : NonCopyable
{
public:
    Buffer(LogicalDevice* device, vk::DeviceSize m_instanceSize, n32 m_instanceCount, vk::BufferUsageFlags usageFlags,
           vk::MemoryPropertyFlags memoryPropertyFlags, VmaMemoryUsage memoryUsage, vk::DeviceSize minOffsetAlignment = 1);
    Buffer();
    ~Buffer();

    vk::Result Map(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);
    void       UnMap();

    void Init(LogicalDevice* device, vk::DeviceSize m_instanceSize, n32 m_instanceCount, vk::BufferUsageFlags usageFlags,
              vk::MemoryPropertyFlags memoryPropertyFlags, VmaMemoryUsage memoryUsage, vk::DeviceSize minOffsetAlignment = 1);

    void                     WriteToBuffer(void* data, vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);
    vk::Result               Flush(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);
    vk::DescriptorBufferInfo DescriptorInfo(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0) const;
    vk::Result               Invalidate(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);

    void                     WriteToIndex(void* data, int index);
    vk::Result               FlushIndex(int index);
    vk::DescriptorBufferInfo DescriptorInfoForIndex(int index);
    vk::Result               InvalidateIndex(int index);

    void UpdateAddress(vk::BufferUsageFlags usage);

    vk::Buffer              GetBuffer() const { return m_buffer; }
    void*                   GetMappedMemory() const { return m_allocationInfo.pMappedData; }
    n32                     GetInstanceCount() const { return m_instanceCount; }
    vk::DeviceSize          GetInstanceSize() const { return m_instanceSize; }
    vk::DeviceSize          GetAlignmentSize() const { return m_instanceSize; }
    vk::BufferUsageFlags    GetUsageFlags() const { return m_usageFlags; }
    vk::MemoryPropertyFlags GetMemoryPropertyFlags() const { return m_memoryPropertyFlags; }
    vk::DeviceSize          GetBufferSize() const { return m_bufferSize; }
    vk::DeviceAddress       GetDeviceAddress()
    {
        UpdateAddress(m_usageFlags);
        return m_deviceAddress;
    }

    static void CopyBuffer(LogicalDevice& device, Buffer& srcBuffer, Buffer& dstBuffer, vk::DeviceSize size);

    bool IsMapped() const { return m_mapCallCount > 0 ? true : false; }

private:
    struct CreateInfo
    {
        LogicalDevice*          device;
        vk::DeviceSize          size;
        vk::BufferUsageFlags    bufferUsage;
        VmaMemoryUsage          memoryUsage;
        vk::MemoryPropertyFlags properties;
        vk::Buffer*             buffer;
        vk::DeviceMemory        memory;
        void*                   data;
        VmaAllocation&          allocation;
        vk::DeviceSize          minOffsetAlignment = 1;
    };
    static vk::DeviceSize GetAlignment(vk::DeviceSize instanceSize, vk::DeviceSize minOffsetAlignment);
    void                  CreateBuffer(CreateInfo& createInfo);

    LogicalDevice*    m_logicalDevice;
    vk::Buffer        m_buffer = VK_NULL_HANDLE;
    VmaAllocation     m_allocation;
    VmaAllocationInfo m_allocationInfo;
    vk::DeviceAddress m_deviceAddress;

    vk::DeviceSize          m_bufferSize;
    n32                     m_instanceCount;
    vk::DeviceSize          m_instanceSize;
    vk::DeviceSize          m_alignmentSize;
    vk::BufferUsageFlags    m_usageFlags;
    vk::MemoryPropertyFlags m_memoryPropertyFlags;

    // safe gaurd, in case Buffer::Map() gets called more than once
    int m_mapCallCount{0};
};
} // namespace Humongous
