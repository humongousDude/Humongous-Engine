#pragma once

#include "non_copyable.hpp"
#include <glm/fwd.hpp>
#include <logical_device.hpp>
#include <vk_mem_alloc.h>

namespace Humongous
{
class Buffer : NonCopyable
{
public:
    Buffer(LogicalDevice& device, VkDeviceSize m_instanceSize, uint32_t m_instanceCount, VkBufferUsageFlags usageFlags,
           VkMemoryPropertyFlags memoryPropertyFlags, VmaMemoryUsage memoryUsage, VkDeviceSize minOffsetAlignment = 1);
    ~Buffer();

    VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void     UnMap();

    void                   WriteToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult               Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkDescriptorBufferInfo DescriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult               Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    void                   WriteToIndex(void* data, int index);
    VkResult               FlushIndex(int index);
    VkDescriptorBufferInfo DescriptorInfoForIndex(int index);
    VkResult               InvalidateIndex(int index);

    void UpdateAddress(VkBufferUsageFlags usage);

    VkBuffer              GetBuffer() const { return m_buffer; }
    void*                 GetMappedMemory() const { return m_allocationInfo.pMappedData; }
    uint32_t              GetInstanceCount() const { return m_instanceCount; }
    VkDeviceSize          GetInstanceSize() const { return m_instanceSize; }
    VkDeviceSize          GetAlignmentSize() const { return m_instanceSize; }
    VkBufferUsageFlags    GetUsageFlags() const { return m_usageFlags; }
    VkMemoryPropertyFlags GetMemoryPropertyFlags() const { return m_memoryPropertyFlags; }
    VkDeviceSize          GetBufferSize() const { return m_bufferSize; }
    VkDeviceAddress       GetDeviceAddress() const { return m_deviceAddress; }

    static void CopyBuffer(LogicalDevice& device, Buffer& srcBuffer, Buffer& dstBuffer, VkDeviceSize size);

private:
    struct CreateInfo
    {
        LogicalDevice&        device;
        VkDeviceSize          size;
        VkBufferUsageFlags    bufferUsage;
        VmaMemoryUsage        memoryUsage;
        VkMemoryPropertyFlags properties;
        VkBuffer*             buffer;
        VkDeviceMemory        memory;
        void*                 data;
        VmaAllocation&        allocation;
        VkDeviceSize          minOffsetAlignment = 1;
    };
    static VkDeviceSize GetAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);
    void                CreateBuffer(CreateInfo& createInfo);

    LogicalDevice&    m_logicalDevice;
    VkBuffer          m_buffer = VK_NULL_HANDLE;
    VmaAllocation     m_allocation;
    VmaAllocationInfo m_allocationInfo;
    VkDeviceAddress   m_deviceAddress;

    VkDeviceSize          m_bufferSize;
    uint32_t              m_instanceCount;
    VkDeviceSize          m_instanceSize;
    VkDeviceSize          m_alignmentSize;
    VkBufferUsageFlags    m_usageFlags;
    VkMemoryPropertyFlags m_memoryPropertyFlags;

    // safe gaurd, in case Buffer::Map() gets called more than once
    int m_mapCallCount{0};
};
} // namespace Humongous
