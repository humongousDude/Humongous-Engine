#pragma once

#include "logical_device.hpp"
#include "singleton.hpp"

namespace Humongous
{
class Allocator : public Singleton<Allocator>
{
public:
    static void Initialize(const LogicalDevice& logicalDevice) { Get().Internal_Initialize(logicalDevice); }
    static void Shutdown() { Get().Internal_Shutdown(); }

    static VmaPool& GetBufferPool() { return Get().Internal_GetBufferPool(); }
    static VmaPool& GetglTFImagePool() { return Get().Internal_GetglTFImagePool(); }

private:
    bool m_initialized = false;

    const LogicalDevice* m_logicalDevice;

    VmaPool m_vertexBufPool;

    VmaPool m_gltfImgPool;
    VmaPool m_2dImgPool;

    VmaPool m_cubeMapPool;

    void     Internal_Initialize(const LogicalDevice& logicalDevice);
    void     Internal_Shutdown();
    VmaPool& Internal_GetBufferPool() { return m_vertexBufPool; };
    VmaPool& Internal_GetglTFImagePool() { return m_gltfImgPool; };
};
} // namespace Humongous
