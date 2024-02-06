#pragma once

#include "non_copyable.hpp"
#include <logical_device.hpp>
#include <vk_mem_alloc.h>

namespace Humongous
{
class Buffer : NonCopyable
{
public:
    Buffer(LogicalDevice& device, VkDeviceSize instanceSize, uint32_t instanceCount, VkBufferUsageFlags usageFlags,
           VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize minOffsetAlignment = 1);
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

    VkBuffer              GetBuffer() const { return m_buffer; }
    void*                 GetMappedMemory() const { return m_mapped; }
    uint32_t              GetInstanceCount() const { return m_instanceCount; }
    VkDeviceSize          GetInstanceSize() const { return m_instanceSize; }
    VkDeviceSize          GetAlignmentSize() const { return m_instanceSize; }
    VkBufferUsageFlags    GetUsageFlags() const { return m_usageFlags; }
    VkMemoryPropertyFlags GetMemoryPropertyFlags() const { return m_memoryPropertyFlags; }
    VkDeviceSize          GetBufferSize() const { return m_bufferSize; }

private:
    struct CreateInfo
    {
        LogicalDevice&        device;
        VkDeviceSize          size;
        VkBufferUsageFlags    usage;
        VkMemoryPropertyFlags properties;
        VkBuffer*             buffer;
        VkDeviceMemory        memory;
        void*                 data;
        VmaAllocation&        allocation;
    };
    static VkDeviceSize GetAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);
    void                CreateBuffer(CreateInfo& createInfo);

    void*          m_mapped;
    LogicalDevice& m_logicalDevice;
    VkBuffer       m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VmaAllocation  m_allocation;

    VkDeviceSize          m_bufferSize;
    uint32_t              m_instanceCount;
    VkDeviceSize          m_instanceSize;
    VkDeviceSize          m_alignmentSize;
    VkBufferUsageFlags    m_usageFlags;
    VkMemoryPropertyFlags m_memoryPropertyFlags;
};
} // namespace Humongous
