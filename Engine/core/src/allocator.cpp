#define VMA_IMPLEMENTATION
#include "logger.hpp"

#include "allocator.hpp"
#include "logical_device.hpp"

namespace Humongous
{

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"

void VKAPI_PTR VmaAllocateDeviceMemoryFunction(VmaAllocator allocator, u32 memoryType, VkDeviceMemory memory, VkDeviceSize size, void* pUserData)
{
    Allocator::VMAData* myUserData = static_cast<Allocator::VMAData*>(pUserData);
    if(myUserData) { myUserData->allocationCount++; }

    HGTRACE("VMA_ALLOC_CB: Allocated memoryType=%u, memory=0x%p, size=%llu bytes. Total allocations: %d", memoryType, (void*)memory,
            (unsigned long long)size, myUserData ? myUserData->allocationCount : -1);
}

void VKAPI_PTR VmaFreeDeviceMemoryFunction(VmaAllocator allocator, u32 memoryType, VkDeviceMemory memory, VkDeviceSize size, void* pUserData)
{
    Allocator::VMAData* myUserData = static_cast<Allocator::VMAData*>(pUserData);
    if(myUserData) { myUserData->freeCount++; }

    HGTRACE("VMA_FREE_CB: Freeing memoryType=%u, memory=0x%p, size=%llu bytes. Total frees: %d", memoryType, (void*)memory,
            (unsigned long long)size, myUserData ? myUserData->freeCount : -1);
}

#pragma GCC diagnostic pop

Allocator::Allocator(const ILogicalDevice& logicalDevice, const class IInstance& instance) : m_logicalDevice{logicalDevice}, m_instance{instance}
{
    HGINFO("Initializing allocator...");
    Initialize();
    HGINFO("Allocator initialized");
};

Allocator::~Allocator()
{
    HGINFO("Shutting down allocator...");
    vmaDestroyAllocator(m_allocator);
    HGINFO("Allocator shutdown");
}

void Allocator::Initialize()
{
    VmaDeviceMemoryCallbacks memoryCallbacks = {};
    memoryCallbacks.pfnAllocate = VmaAllocateDeviceMemoryFunction;
    memoryCallbacks.pfnFree = VmaFreeDeviceMemoryFunction;
    memoryCallbacks.pUserData = &m_vmaData;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = m_logicalDevice.GetPhysicalDevice().GetVkPhysicalDevice();
    allocatorInfo.device = m_logicalDevice.GetVkDevice();
    allocatorInfo.instance = m_instance.GetVkInstance();
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorInfo.pDeviceMemoryCallbacks = &memoryCallbacks;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);
}

vk::Result Allocator::AllocateBuffer(const vk::DeviceSize size, const vk::BufferUsageFlags bufferUsage, VmaAllocationCreateInfo allocCreateInfo,
                                     VmaAllocation& allocation, vk::Buffer& buffer)
{
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = size;
    bufferInfo.usage = bufferUsage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    vk::Result result = static_cast<vk::Result>(vmaCreateBuffer(m_allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocCreateInfo,
                                                                reinterpret_cast<VkBuffer*>(&buffer), &allocation, nullptr));

    m_bufferCount++;

    if(result != vk::Result::eSuccess) { buffer = VK_NULL_HANDLE; }
    return result;
}

void Allocator::FreeBuffer(VmaAllocation allocation, vk::Buffer buffer)
{
    vmaDestroyBuffer(m_allocator, buffer, allocation);
    m_bufferCount--;
}

vk::Result Allocator::AllocateImage(const vk::ImageCreateInfo& createInfo, VmaAllocationCreateInfo allocCreateInfo, VmaAllocation& allocation,
                                    vk::Image& image)
{
    auto res = vk::Result(vmaCreateImage(m_allocator, reinterpret_cast<const VkImageCreateInfo*>(&createInfo), &allocCreateInfo,
                                         reinterpret_cast<VkImage*>(&image), &allocation, nullptr));

    if(res != vk::Result::eSuccess) { image = VK_NULL_HANDLE; }

    m_imageCount++;
    return res;
}

void Allocator::FreeImage(VmaAllocation allocation, vk::Image image)
{
    vmaDestroyImage(m_allocator, image, allocation);
    m_imageCount--;
}

void Allocator::NameAllocation(VmaAllocation allocation, const char* name) { vmaSetAllocationName(m_allocator, allocation, name); }

VmaAllocationInfo Allocator::GetAllocationInfo(VmaAllocation allocation)
{
    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(m_allocator, allocation, &allocInfo);
    return allocInfo;
}

vk::Result Allocator::Map(VmaAllocation allocation, void** data) { return vk::Result(vmaMapMemory(m_allocator, allocation, data)); }

void Allocator::Unmap(VmaAllocation allocation) { vmaUnmapMemory(m_allocator, allocation); }

vk::Result Allocator::Invalidate(VmaAllocation allocation, vk::DeviceSize offset, vk::DeviceSize size)
{
    return vk::Result(vmaInvalidateAllocation(m_allocator, allocation, offset, size));
}

} // namespace Humongous
