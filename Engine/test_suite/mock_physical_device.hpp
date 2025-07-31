#pragma once

#include "physical_device.hpp"
#include "gmock/gmock.h"

namespace Humongous
{

class MockPhysicalDevice : public IPhysicalDevice
{
public:
    MOCK_METHOD(vk::PhysicalDevice, GetVkPhysicalDevice, (), (const, override));

    MOCK_METHOD(QueueFamilyData, FindQueueFamilies, (vk::PhysicalDevice physicalDevice), (const, override));

    MOCK_METHOD(std::vector<const char*>, GetDeviceExtensions, (), (const, override));

    MOCK_METHOD(SwapChainSupportDetails, QuerySwapChainSupport, (vk::PhysicalDevice physicalDevice), (const, override));

    MOCK_METHOD(vk::SurfaceKHR, GetSurface, (), (const, override));

    MOCK_METHOD(vk::PhysicalDeviceProperties2, GetProperties, (), (const, override));

    MOCK_METHOD(VkPhysicalDeviceFeatures2, GetFeatures, (), (const, override));

    MOCK_METHOD(DeviceSupportLevel, GetCurrentSupportLevel, (), (const, override));

    MOCK_METHOD(const DeviceCapabilities&, GetCurrentCapabilities, (), (const, override));

    MockPhysicalDevice()
    {
        ON_CALL(*this, GetDeviceExtensions()).WillByDefault(testing::Return(m_deviceExtensions));
        ON_CALL(*this, GetCurrentCapabilities()).WillByDefault(testing::ReturnRef(m_deviceCapabilities));
    }

    mutable DeviceCapabilities       m_deviceCapabilities;
    mutable std::vector<const char*> m_deviceExtensions;
};

} // namespace Humongous
