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
        ON_CALL(*this, AllocateBuffer(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault([this](const vk::DeviceSize size, const vk::BufferUsageFlags bufferUsage, VmaAllocationCreateInfo allocCreateInfo,
                                  VmaAllocation& allocation, vk::Buffer& buffer) {
                VmaAllocation newAllocation = reinterpret_cast<VmaAllocation>(m_nextAllocationHandle++);

                m_allocations[newAllocation].buffer = std::vector<char>(size);

                allocation = newAllocation;
                buffer = vk::Buffer(reinterpret_cast<VkBuffer>(0xAABBCCDD));
                return vk::Result::eSuccess;
            });

        ON_CALL(*this, FreeBuffer(::testing::_, ::testing::_)).WillByDefault([this](VmaAllocation allocation, vk::Buffer) {
            if(m_allocations.count(allocation)) { m_allocations.erase(allocation); }
        });

        ON_CALL(*this, Map(::testing::_, ::testing::_)).WillByDefault([this](VmaAllocation allocation, void** data) {
            auto it = m_allocations.find(allocation);
            if(it == m_allocations.end())
            {
                *data = nullptr;
                return vk::Result::eErrorMemoryMapFailed;
            }

            const auto& props = it->second.properties;
            if(props & vk::MemoryPropertyFlagBits::eHostVisible)
            {
                *data = it->second.buffer.data();
                return vk::Result::eSuccess;
            }

            *data = nullptr;
            return vk::Result::eErrorMemoryMapFailed;
        });

        ON_CALL(*this, GetAllocationInfo(::testing::_)).WillByDefault([this](VmaAllocation allocation) -> VmaAllocationInfo {
            VmaAllocationInfo info{};
            if(m_allocations.count(allocation))
            {
                info.pMappedData = m_allocations.at(allocation).buffer.data();
                info.size = m_allocations.at(allocation).buffer.size();
            }
            return info;
        });

        ON_CALL(*this, FreeBuffer(::testing::_, ::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, AllocateImage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<2>(reinterpret_cast<VmaAllocation>(0xCAFEFACE)),
                                            ::testing::SetArgReferee<3>(vk::Image(reinterpret_cast<VkImage>(0xCCDD1122))),
                                            ::testing::Return(vk::Result::eSuccess)));

        ON_CALL(*this, FreeImage(::testing::_, ::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, NameAllocation(::testing::_, ::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, Unmap(::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, Invalidate(::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Return(vk::Result::eSuccess));
    };

    MOCK_METHOD(vk::Result, AllocateBuffer,
                (const vk::DeviceSize size, const vk::BufferUsageFlags bufferUsage, VmaAllocationCreateInfo allocCreateInfo,
                 VmaAllocation& allocation, vk::Buffer& buffer),
                (override));
    MOCK_METHOD(void, FreeBuffer, (VmaAllocation allocation, vk::Buffer buffer), (override));
    MOCK_METHOD(vk::Result, AllocateImage,
                (const vk::ImageCreateInfo& createInfo, VmaAllocationCreateInfo a, VmaAllocation& allocation, vk::Image& image), (override));
    MOCK_METHOD(void, FreeImage, (VmaAllocation allocation, vk::Image image), (override));
    MOCK_METHOD(void, NameAllocation, (VmaAllocation allocation, const char* name), (override));
    MOCK_METHOD(VmaAllocationInfo, GetAllocationInfo, (VmaAllocation allocation), (override));
    MOCK_METHOD(vk::Result, Map, (VmaAllocation allocation, void** data), (override));
    MOCK_METHOD(void, Unmap, (VmaAllocation allocation), (override));
    MOCK_METHOD(vk::Result, Invalidate, (VmaAllocation allocation, vk::DeviceSize offset, vk::DeviceSize size), (override));

private:
    uintptr_t m_nextAllocationHandle = 1;

    struct MockAllocation
    {
        std::vector<char>       buffer;
        vk::MemoryPropertyFlags properties;
    };

    std::unordered_map<VmaAllocation, MockAllocation> m_allocations;
};

} // namespace Humongous
