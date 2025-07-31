#pragma once

#include "abstractions/buffer.hpp"
#include "images.hpp"
#include "logical_device.hpp"

namespace Humongous
{
constexpr static u32 INVALID_ID = -1;

class RenderGraphPass;
class RenderGraphRegistry;
class RenderGraph;

enum class ResourceUsage
{
    Read,
    Write,
    ReadWrite
};

struct ImageHandle
{
    u32 id = INVALID_ID;
};

struct BufferHandle
{
    u32 id = INVALID_ID;
};

struct ImageResource
{
    u32 id = INVALID_ID;
    b8  isTransient = false;

    Utils::AllocatedImageCreateInfo* imageCreateInfo = nullptr;
    AllocatedImage*                  externalImage = nullptr;

    u32 firstAccessPassId = INVALID_ID;
    u32 lastAccessPassId = 0;

    vk::ImageLayout currentLayout = vk::ImageLayout::eUndefined;
    vk::ImageLayout requiredLayout = vk::ImageLayout::eUndefined;
};

struct ImageIdentity
{
    u32 id = INVALID_ID;
    u32 width, height, mipLevels;

    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
};

struct BufferResource
{
    u32 id = INVALID_ID;
    b8  isTransient = false;

    Buffer::BufferCreateInfo* bufferCreateInfo = nullptr;
    Buffer*                   externalBuffer = nullptr;

    u32 firstAccessPassId = INVALID_ID;
    u32 lastAccessPassId = 0;
};

struct BufferIdentity
{
    u32                     id = INVALID_ID;
    u32                     size;
    vk::BufferUsageFlags    usage = vk::BufferUsageFlagBits::eTransferSrc;
    vk::MemoryPropertyFlags memoryProperty = vk::MemoryPropertyFlagBits::eDeviceLocal;
};

class RenderGraphPass
{
public:
    // probably not needed
    // void ReadsFromImage(const ImageHandle& imageHandle);
    // void ReadsFromBuffer(const BufferHandle& bufferHandle);
    // void WritesToImage(const ImageHandle& imageHandle);
    // void WritesToBuffer(const BufferHandle& bufferHandle);

    ImageHandle  AddTransientImage(RenderGraphRegistry& registry, const Utils::AllocatedImageCreateInfo& imageCreateInfo,
                                   const ResourceUsage& usage);
    BufferHandle AddTransientBuffer(RenderGraphRegistry& registry, const Buffer::BufferCreateInfo& bufferCreateInfo, const ResourceUsage& usage);

    ImageHandle ImportImage(RenderGraphRegistry& registry, AllocatedImage* image, const vk::ImageLayout& initialLayout, const ResourceUsage& usage);
    BufferHandle ImportBuffer(RenderGraphRegistry& registry, Buffer* buffer, const ResourceUsage& usage);

    void SetExecuteCallback(std::function<void(vk::CommandBuffer, const RenderGraphRegistry&)>&& execute);
    std::function<void(vk::CommandBuffer, const RenderGraphRegistry&)> m_executeCallback;

private:
    friend class RenderGraph;

    std::string               name;
    std::vector<ImageHandle>  m_readsFromImage;
    std::vector<BufferHandle> m_readsFromBuffer;
    std::vector<ImageHandle>  m_writesToImage;
    std::vector<BufferHandle> m_writesToBuffer;

    u32 passId = INVALID_ID;
};

class RenderGraphRegistry
{
public:
    RenderGraphRegistry(const ILogicalDevice& logicalDevice);
    ~RenderGraphRegistry();

    const ImageResource&  GetImageResource(ImageResource handle);
    const BufferResource& GetBufferResource(BufferResource handle);

    ImageHandle  RegisterTransientImage(const ImageIdentity& identity);
    BufferHandle RegisterTransientBuffer(const BufferIdentity& identity);
    ImageHandle  RegisterImportedImage(AllocatedImage* image, const ImageIdentity& identity);
    BufferHandle RegisterImportedBuffer(Buffer* buffer, const BufferIdentity& identity);

    void Reset();

private:
    const ILogicalDevice& m_logicalDevice;

    u32                                     m_nextImageId = 0;
    u32                                     m_nextBufferId = 0;
    std::unordered_map<u32, ImageResource>  m_imageResources;
    std::unordered_map<u32, BufferResource> m_bufferResources;
};

class RenderGraph
{
public:
    RenderGraph(const ILogicalDevice& logicalDevice);
    ~RenderGraph();

    RenderGraphPass& AddPass(const std::string& name);
    void             Compile();
    void             Execute(vk::CommandBuffer commandBuffer);
    void             Reset();

    RenderGraphRegistry& GetRegistry() { return m_registry; }

private:
    const ILogicalDevice& m_logicalDevice;
    RenderGraphRegistry   m_registry;

    u32                          m_nextPassId = 0;
    std::vector<RenderGraphPass> m_passes;

    std::vector<RenderGraphPass*> m_executionOrderPasses;
};

} // namespace Humongous
