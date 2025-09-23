// Original from Brendan Galea's vulkan tutorial, adapted to use VMA
#pragma once

#include "abstractions/image.hpp"
#include "logical_device.hpp"
#include "non_copyable.hpp"
#include <vk_mem_alloc.h>

namespace Humongous
{
class Buffer : NonCopyable
{
public:
    struct BufferCreateInfo
    {
        const ILogicalDevice&   device;
        vk::DeviceSize          size = 1;
        u32                     instanceCount = 1;
        vk::BufferUsageFlags    bufferUsage;
        VmaMemoryUsage          memoryUsage = VMA_MEMORY_USAGE_AUTO;
        vk::MemoryPropertyFlags properties;
        vk::DeviceSize          minOffsetAlignment = 1;
        std::string             name = "";
        vk::SharingMode         sharingMode = vk::SharingMode::eExclusive;
        std::vector<u32>        queueFamilyIndices;
    };

    Buffer(const BufferCreateInfo& createInfo);
    ~Buffer();

    vk::Result Map(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);
    void       UnMap();

    void Init(const BufferCreateInfo& createInfo);

    void                     WriteToBuffer(void* data, vk::DeviceSize size = vk::WholeSize, vk::DeviceSize offset = 0);
    vk::Result               Flush(vk::DeviceSize size = vk::WholeSize, vk::DeviceSize offset = 0);
    vk::DescriptorBufferInfo DescriptorInfo(vk::DeviceSize size = vk::WholeSize, vk::DeviceSize offset = 0) const;
    vk::Result               Invalidate(vk::DeviceSize size = vk::WholeSize, vk::DeviceSize offset = 0);

    void                     WriteToIndex(void* data, int index);
    vk::Result               FlushIndex(int index);
    vk::DescriptorBufferInfo DescriptorInfoForIndex(int index);
    vk::Result               InvalidateIndex(int index);

    void UpdateAddress(vk::BufferUsageFlags usage);

    vk::Buffer              GetBuffer() const { return m_buffer; }
    void*                   GetMappedMemory() const { return m_allocationInfo.pMappedData; }
    u32                     GetInstanceCount() const { return m_instanceCount; }
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

    static void CopyBuffer(const ILogicalDevice& device, Buffer& srcBuffer, Buffer& dstBuffer, vk::DeviceSize size);

    b8 IsMapped() const { return m_isMapped; }
    b8 IsValid() const { return m_isValid; }

    void TransferQueue(const vk::CommandBuffer& cmd, const u32& srcQueueFamilyIndex, const u32& dstQueueFamilyIndex,
                       const vk::PipelineStageFlags2& srcStage, const vk::PipelineStageFlags2& dstStage, const vk::AccessFlags2& srcAccess,
                       const vk::AccessFlags2& dstAccess);

    void CopyToImage(vk::CommandBuffer cmd, Image& image, const std::vector<vk::BufferImageCopy>& regions);
    void CopyToImage(vk::CommandBuffer cmd, Image& image, const u32& width, const u32& height);

private:
    static vk::DeviceSize GetAlignment(vk::DeviceSize instanceSize, vk::DeviceSize minOffsetAlignment);

    const ILogicalDevice& m_logicalDevice;
    vk::Buffer            m_buffer = VK_NULL_HANDLE;
    VmaAllocation         m_allocation;
    VmaAllocationInfo     m_allocationInfo;
    vk::DeviceAddress     m_deviceAddress;

    vk::DeviceSize          m_bufferSize;
    u32                     m_instanceCount;
    vk::DeviceSize          m_instanceSize;
    vk::DeviceSize          m_alignmentSize;
    vk::BufferUsageFlags    m_usageFlags;
    vk::MemoryPropertyFlags m_memoryPropertyFlags;
    std::string             m_name = "";
    b8                      m_isValid = false;
    b8                      m_isMapped{false};
    vk::SharingMode         m_sharingMode = vk::SharingMode::eExclusive;
    std::vector<u32>        m_queueFamilyIndices{0};
};
} // namespace Humongous
