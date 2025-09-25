#include "abstractions/buffer.hpp"
#include "mock_logical_device.hpp"
#include "gtest/gtest.h"

namespace Humongous
{

TEST(BufferSuite, CreateBuffer)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eDeviceLocal);
}

TEST(BufferSuite, ExtremelyLargeBuffer)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};

    const u32 size = 1024 * 1024 * 1024;

    info.size = size;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), size);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eDeviceLocal);
}

TEST(BufferSuite, CreateBufferWithInvalidSize)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 0;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), false);
    EXPECT_EQ(buffer.GetBufferSize(), 0);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eDeviceLocal);
}

TEST(BufferSuite, CreateBufferWithInvalidProperties)
{
    {

        // The vulkan spec does not allow the combination of eProtected and eHostVisible
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eProtected | vk::MemoryPropertyFlagBits::eHostVisible;

        testing::NiceMock<MockLogicalDevice> device;
        Buffer::BufferCreateInfo             info{device};
        info.size = 1024;
        info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
        info.properties = properties;

        Buffer buffer{info};

        EXPECT_EQ(buffer.IsValid(), false);
        EXPECT_EQ(buffer.GetBufferSize(), 1024);
        EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
        EXPECT_EQ(buffer.GetMemoryPropertyFlags(), properties);
    }

    {
        // The vulkan spec does not allow the combination of eProtected and eHostCoherent
        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eProtected | vk::MemoryPropertyFlagBits::eHostCoherent;

        testing::NiceMock<MockLogicalDevice> device;
        Buffer::BufferCreateInfo             info{device};
        info.size = 1024;
        info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
        info.properties = properties;

        Buffer buffer{info};

        EXPECT_EQ(buffer.IsValid(), false);
        EXPECT_EQ(buffer.GetBufferSize(), 1024);
        EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
        EXPECT_EQ(buffer.GetMemoryPropertyFlags(), properties);
    }
}

TEST(BufferSuite, ValidMapCall)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_EQ(buffer.Map(), vk::Result::eSuccess);
    EXPECT_EQ(buffer.IsMapped(), true);
}

TEST(BufferSuite, MapCallToDeviceLocalBuffer)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eDeviceLocal);
    EXPECT_NE(buffer.Map(), vk::Result::eSuccess);
    EXPECT_EQ(buffer.IsMapped(), false);
}

TEST(BufferSuite, UnmapCallToUnmappedBuffer)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    buffer.UnMap();
    EXPECT_EQ(buffer.IsMapped(), false);
}

TEST(BufferSuite, UnmapCallToMappedBuffer)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_EQ(buffer.Map(), vk::Result::eSuccess);
    EXPECT_EQ(buffer.IsMapped(), true);
    buffer.UnMap();
    EXPECT_EQ(buffer.IsMapped(), false);
}

TEST(BufferSuite, WriteToBuffer)
{
    testing::NiceMock<MockLogicalDevice> device;
    vk::DeviceSize                       targetSize = sizeof(u32);

    Buffer::BufferCreateInfo info{device};
    info.size = targetSize;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), targetSize);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_EQ(buffer.Map(), vk::Result::eSuccess);
    EXPECT_EQ(buffer.IsMapped(), true);

    u32 data = 0;
    buffer.WriteToBuffer(&data, targetSize);

    EXPECT_EQ(data, 0);
}

TEST(BufferSuite, WriteToBufferWithSize)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_EQ(buffer.Map(), vk::Result::eSuccess);
    EXPECT_EQ(buffer.IsMapped(), true);

    u32 data = 0;
    buffer.WriteToBuffer(&data, sizeof(u32));

    EXPECT_EQ(data, 0);
}

TEST(BufferSuite, WriteToBufferWithOffset)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_EQ(buffer.Map(), vk::Result::eSuccess);
    EXPECT_EQ(buffer.IsMapped(), true);

    u32 data = 0;
    buffer.WriteToBuffer(&data, sizeof(u32), sizeof(u32));
}

TEST(BufferSuite, WriteToUnmappedBuffer)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);

    u32 data = 0;
    buffer.WriteToBuffer(&data, sizeof(u32), sizeof(u32));
}

TEST(BufferSuite, WriteToBufferWithInvalidSize)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    vk::DeviceSize                       targetSize = sizeof(u32);
    info.size = targetSize;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), targetSize);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_EQ(buffer.Map(), vk::Result::eSuccess);
    EXPECT_EQ(buffer.IsMapped(), true);

    u32 data = 0;
    buffer.WriteToBuffer(&data, targetSize + 1);
}

TEST(BufferSuite, WriteToBufferWithInvalidOffset)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    vk::DeviceSize                       targetSize = sizeof(u32);

    info.size = targetSize;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), targetSize);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_EQ(buffer.Map(), vk::Result::eSuccess);
    EXPECT_EQ(buffer.IsMapped(), true);

    u32 data = 0;
    buffer.WriteToBuffer(&data, 0, targetSize + 1);
}

TEST(BufferSuite, WriteToBufferWithOversizedSize)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_EQ(buffer.Map(), vk::Result::eSuccess);
    EXPECT_EQ(buffer.IsMapped(), true);

    u32 data = 0;
    buffer.WriteToBuffer(&data, sizeof(u32) * 1024);
}

TEST(BufferSuite, WriteToBufferWithOversizedOffset)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_EQ(buffer.Map(), vk::Result::eSuccess);
    EXPECT_EQ(buffer.IsMapped(), true);

    u32 data = 0;
    buffer.WriteToBuffer(&data, sizeof(u32), sizeof(u32) * 1024);
}

TEST(BufferSuite, FlushCallToUnmappedBuffer)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_NE(buffer.Flush(), vk::Result::eSuccess);
}

TEST(BufferSuite, FlushCallToMappedBuffer)
{
    testing::NiceMock<MockLogicalDevice> device;
    Buffer::BufferCreateInfo             info{device};
    info.size = 1024;
    info.bufferUsage = vk::BufferUsageFlagBits::eTransferDst;
    info.properties = vk::MemoryPropertyFlagBits::eHostVisible;

    Buffer buffer{info};

    EXPECT_EQ(buffer.IsValid(), true);
    EXPECT_EQ(buffer.GetBufferSize(), 1024);
    EXPECT_EQ(buffer.GetUsageFlags(), vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_EQ(buffer.GetMemoryPropertyFlags(), vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_EQ(buffer.Map(), vk::Result::eSuccess);
    EXPECT_EQ(buffer.IsMapped(), true);
    EXPECT_EQ(buffer.Flush(), vk::Result::eSuccess);
}

} // namespace Humongous
