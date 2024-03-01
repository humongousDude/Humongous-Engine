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

    VmaPool& GetBufferPool() { return m_vertexBufPool; };
    VmaPool& GetglTFImagePool() { return m_gltfImgPool; };

private:
    bool m_initialized = false;

    LogicalDevice* m_logicalDevice = nullptr;

    VmaPool m_vertexBufPool;

    VmaPool m_gltfImgPool;
    VmaPool m_2dImgPool;

    // Unusued for now
    VmaPool m_cubeMapPool;
};
} // namespace Humongous
