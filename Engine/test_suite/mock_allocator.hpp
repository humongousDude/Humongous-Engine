#pragma once

#include "allocator.hpp"
#include "gmock/gmock.h"

namespace Humongous
{

class MockAllocator : public IAllocator
{
public:
    MockAllocator()
    {
        // Allocate a buffer of the requested size and store it.
        ON_CALL(*this, AllocateBuffer(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault([this](const vk::DeviceSize size, const VmaMemoryUsage, const vk::BufferUsageFlags, const vk::MemoryPropertyFlags,
                                  VmaAllocation& allocation, vk::Buffer& buffer) {
                // Create a unique handle for this allocation
                VmaAllocation newAllocation = reinterpret_cast<VmaAllocation>(m_nextAllocationHandle++);

                // Allocate a buffer of the correct size
                m_allocations[newAllocation] = std::vector<char>(size);

                // Return the mock handles
                allocation = newAllocation;
                buffer = vk::Buffer(reinterpret_cast<VkBuffer>(0xAABBCCDD)); // Dummy handle
                return vk::Result::eSuccess;
            });

        // Free the buffer by removing it from our map.
        ON_CALL(*this, FreeBuffer(::testing::_, ::testing::_)).WillByDefault([this](VmaAllocation allocation, vk::Buffer) {
            if(m_allocations.count(allocation)) { m_allocations.erase(allocation); }
        });

        // Return a pointer to the correct allocation's data.
        ON_CALL(*this, Map(::testing::_, ::testing::_)).WillByDefault([this](VmaAllocation allocation, void** data) {
            if(m_allocations.count(allocation))
            {
                *data = m_allocations.at(allocation).data();
                return vk::Result::eSuccess;
            }
            *data = nullptr;
            return vk::Result::eErrorMemoryMapFailed; // Or another appropriate error
        });

        // Also update GetAllocationInfo to return the correct pointer.
        ON_CALL(*this, GetAllocationInfo(::testing::_)).WillByDefault([this](VmaAllocation allocation) -> VmaAllocationInfo {
            VmaAllocationInfo info{};
            if(m_allocations.count(allocation))
            {
                info.pMappedData = m_allocations.at(allocation).data();
                info.size = m_allocations.at(allocation).size();
            }
            return info;
        });

        ON_CALL(*this, FreeBuffer(::testing::_, ::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, AllocateImage(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(reinterpret_cast<VmaAllocation>(0xCAFEFACE)),
                                            ::testing::SetArgReferee<2>(vk::Image(reinterpret_cast<VkImage>(0xCCDD1122))),
                                            ::testing::Return(vk::Result::eSuccess)));

        ON_CALL(*this, FreeImage(::testing::_, ::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, NameAllocation(::testing::_, ::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, Unmap(::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, Invalidate(::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Return(vk::Result::eSuccess));
    };

    MOCK_METHOD(vk::Result, AllocateBuffer,
                (const vk::DeviceSize size, const VmaMemoryUsage vmaUsage, const vk::BufferUsageFlags bufferUsage,
                 const vk::MemoryPropertyFlags properties, VmaAllocation& allocation, vk::Buffer& buffer),
                (override));
    MOCK_METHOD(void, FreeBuffer, (VmaAllocation allocation, vk::Buffer buffer), (override));
    MOCK_METHOD(vk::Result, AllocateImage, (const vk::ImageCreateInfo& createInfo, VmaAllocation& allocation, vk::Image& image), (override));
    MOCK_METHOD(void, FreeImage, (VmaAllocation allocation, vk::Image image), (override));
    MOCK_METHOD(void, NameAllocation, (VmaAllocation allocation, const char* name), (override));
    MOCK_METHOD(VmaAllocationInfo, GetAllocationInfo, (VmaAllocation allocation), (override));
    MOCK_METHOD(vk::Result, Map, (VmaAllocation allocation, void** data), (override));
    MOCK_METHOD(void, Unmap, (VmaAllocation allocation), (override));
    MOCK_METHOD(vk::Result, Invalidate, (VmaAllocation allocation, vk::DeviceSize offset, vk::DeviceSize size), (override));

private:
    uintptr_t                                            m_nextAllocationHandle = 1;
    std::unordered_map<VmaAllocation, std::vector<char>> m_allocations;
};

} // namespace Humongous
