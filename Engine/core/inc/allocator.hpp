#pragma once

#include "defines.hpp"
#include "vk_mem_alloc.h"
#include "vulkan/vulkan.hpp"

namespace Humongous
{
class IAllocator
{
public:
    virtual ~IAllocator() = default;

    virtual vk::Result        AllocateBuffer(const vk::DeviceSize size, const VmaMemoryUsage vmaUsage, const vk::BufferUsageFlags bufferUsage,
                                             VmaAllocationCreateInfo allocCreateInfo, const vk::MemoryPropertyFlags properties, VmaAllocation& allocation,
                                             vk::Buffer& buffer) = 0;
    virtual void              FreeBuffer(VmaAllocation allocation, vk::Buffer buffer) = 0;
    virtual vk::Result        AllocateImage(const vk::ImageCreateInfo& createInfo, VmaAllocation& allocation, vk::Image& image) = 0;
    virtual void              FreeImage(VmaAllocation allocation, vk::Image image) = 0;
    virtual void              NameAllocation(VmaAllocation allocation, const char* name) = 0;
    virtual VmaAllocationInfo GetAllocationInfo(VmaAllocation allocation) = 0;
    virtual vk::Result        Map(VmaAllocation allocation, void** data) = 0;
    virtual void              Unmap(VmaAllocation allocation) = 0;
    virtual vk::Result        Invalidate(VmaAllocation allocation, vk::DeviceSize offset, vk::DeviceSize size) = 0;

    struct VMAData
    {
        u32 allocationCount = 0;
        u32 freeCount = 0;
    };
};

class Allocator : public IAllocator
{
public:
    Allocator(const class ILogicalDevice& logicalDevice, const class IInstance& instance);
    ~Allocator();

    vk::Result        AllocateBuffer(const vk::DeviceSize size, const VmaMemoryUsage vmaUsage, const vk::BufferUsageFlags bufferUsage,
                                     VmaAllocationCreateInfo allocCreateInfo, const vk::MemoryPropertyFlags properties, VmaAllocation& allocation,
                                     vk::Buffer& buffer) override;
    void              FreeBuffer(VmaAllocation allocation, vk::Buffer buffer) override;
    vk::Result        AllocateImage(const vk::ImageCreateInfo& createInfo, VmaAllocation& allocation, vk::Image& image) override;
    void              FreeImage(VmaAllocation allocation, vk::Image image) override;
    void              NameAllocation(VmaAllocation allocation, const char* name) override;
    VmaAllocationInfo GetAllocationInfo(VmaAllocation allocation) override;
    vk::Result        Map(VmaAllocation allocation, void** data) override;
    void              Unmap(VmaAllocation allocation) override;
    vk::Result        Invalidate(VmaAllocation allocation, vk::DeviceSize offset, vk::DeviceSize size) override;

private:
    b8 m_initialized = false;

    const class ILogicalDevice& m_logicalDevice;
    const class IInstance&      m_instance;
    VmaAllocator                m_allocator;
    VMAData                     m_vmaData;

    void Initialize();
};
} // namespace Humongous
