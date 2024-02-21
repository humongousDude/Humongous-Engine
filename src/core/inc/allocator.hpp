#pragma once

#include "logical_device.hpp"
#include "singleton.hpp"

namespace Humongous
{
class Allocator : public Singleton<Allocator>
{
public:
    void Initialize(LogicalDevice* logicalDevice);
    void Shutdown();

private:
    bool m_initialized = false;

    LogicalDevice* m_logicalDevice = nullptr;

    VmaPool m_vertexBufPool;
    VmaPool m_imagePool;
};
} // namespace Humongous
