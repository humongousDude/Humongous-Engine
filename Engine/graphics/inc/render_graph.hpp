#pragma once
#include "defines.hpp"
#include "logical_device.hpp"
#include "non_copyable.hpp"
#include "render_systems/simple_render_system.hpp"
#include "texture.hpp"
#include <string>
#include <unordered_set>

namespace Humongous
{

class RenderGraph;

class RenderResource
{
public:
    enum class Type
    {
        Buffer,
        Texture
    };
    enum class Usage
    {
        ReadOnly,
        WriteOnly,
        ReadWrite
    };
    enum class Queue
    {
        Graphics,
        Compute,
        Transfer
    };

    RenderResource(Type type, Usage usage, Queue queue, const std::string& name) : m_type(type), m_usage(usage), m_queue(queue), m_name(name) {}

    void AddReader(u32 passId) { m_readers.insert(passId); }
    void AddWriter(u32 passId) { m_writers.insert(passId); }

    const std::unordered_set<u32>& GetReaders() const { return m_readers; }
    const std::unordered_set<u32>& GetWriters() const { return m_writers; }
    std::unordered_set<u32>&       GetReaders() { return m_readers; }
    std::unordered_set<u32>&       GetWriters() { return m_writers; }

    Type               GetType() const { return m_type; }
    Usage              GetUsage() const { return m_usage; }
    Queue              GetQueue() const { return m_queue; }
    const std::string& GetName() const { return m_name; }

private:
    const Type              m_type;
    const Usage             m_usage;
    const Queue             m_queue;
    std::unordered_set<u32> m_readers;
    std::unordered_set<u32> m_writers;
    std::string             m_name;
};

class BufferResource : public RenderResource
{
public:
    struct Info
    {
        Buffer*               buffer{nullptr};
        u64                   size;
        u32                   alignment;
        vk::BufferUsageFlags2 usage;
        VmaMemoryUsage        memoryUsage;
    };

    explicit BufferResource(const Info& info, Usage usage, Queue queue, const std::string& name)
        : RenderResource(Type::Buffer, usage, queue, name), m_info(info)
    {
    }

private:
    Info m_info;
};

class TextureResource : public RenderResource
{
public:
    struct Info
    {
        Texture*            image{nullptr};
        vk::Format          format;
        vk::Extent3D        extent;
        u32                 mipLevels;
        u32                 arrayLayers;
        vk::ImageUsageFlags usage;
    };
    explicit TextureResource(const Info& info, Usage usage, Queue queue, const std::string& name)
        : RenderResource(Type::Texture, usage, queue, name), m_info(info)
    {
    }

    Texture* GetTexture() const { return m_info.image; }

private:
    Info m_info;
};

class RenderPass : NonCopyable
{
public:
    enum class Queue
    {
        Graphics,
        Compute,
        Transfer
    };

    struct Info
    {
        std::string              name;
        u32                      id;
        std::vector<RenderPass*> dependencies{};
        Queue                    queue = Queue::Graphics;
    };
    struct AccessedResource
    {
        vk::PipelineStageFlags2 stages{};
        vk::AccessFlags2        access{};
    };
    struct AccessedBuffer : public AccessedResource
    {
        BufferResource* buffer;
    };
    struct AccessedTexture : public AccessedResource
    {
        TextureResource* texture;
        vk::ImageLayout  layout;
    };

    explicit RenderPass(const Info& info) : m_info(info) {}

    void AddDependency(RenderPass* dep) { m_info.dependencies.push_back(dep); }

    void AddColorOutput(TextureResource* texture, vk::PipelineStageFlags2 stages, vk::AccessFlags2 access)
    {
        colorOutputs.push_back(texture);
        genericTexture.push_back({{stages, access}, texture, vk::ImageLayout::eColorAttachmentOptimal});
        texture->AddWriter(m_info.id);
    }
    void AddResolveOutput(TextureResource* texture, vk::PipelineStageFlags2 stages, vk::AccessFlags2 access)
    {
        resolveOutputs.push_back(texture);
        genericTexture.push_back({{stages, access}, texture, vk::ImageLayout::eColorAttachmentOptimal});
        texture->AddWriter(m_info.id);
    }

    const Info& GetInfo() const { return m_info; }
    Info        GetInfo() { return m_info; }

    std::function<void(const IRenderSystem::RenderData&)> exec = nullptr;

private:
    Info m_info;

    std::vector<TextureResource*> colorOutputs;
    std::vector<TextureResource*> resolveOutputs;
    std::vector<TextureResource*> colorInputs;
    std::vector<TextureResource*> colorScaleInputs;
    std::vector<TextureResource*> storageTextureInputs;
    std::vector<TextureResource*> storageTextureOutputs;
    std::vector<TextureResource*> blitTextureInputs;
    std::vector<TextureResource*> blitTextureOutputs;
    std::vector<TextureResource*> attachmentsInputs;
    std::vector<BufferResource*>  storageOutputs;
    std::vector<BufferResource*>  storageInputs;
    std::vector<BufferResource*>  transferOutputs;
    std::vector<AccessedTexture>  genericTexture;
    std::vector<AccessedBuffer>   genericBuffer;

    friend RenderGraph;
};

class RenderGraph : NonCopyable
{
public:
    struct CompiledResourceAccess
    {
        u32                     passId{0};
        b8                      isWrite{false};
        vk::PipelineStageFlags2 stages{};
        vk::AccessFlags2        access{};

        b8 operator==(const CompiledResourceAccess& other) const noexcept
        {
            return passId == other.passId && isWrite == other.isWrite && stages == other.stages && access == other.access;
        }
    };
    struct CompiledResourceAccessHash
    {
        size_t operator()(const CompiledResourceAccess& a) const noexcept
        {
            auto h1 = std::hash<u32>{}(a.passId);
            auto h2 = std::hash<bool>{}(a.isWrite);
            auto h3 = std::hash<uint64_t>{}(static_cast<uint64_t>(a.stages));
            auto h4 = std::hash<uint64_t>{}(static_cast<uint64_t>(a.access));
            // Combine hashes
            return (((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1)) ^ (h4 << 2);
        }
    };

    struct CompiledBufferAccess : public CompiledResourceAccess
    {
        BufferResource* buffer{nullptr};
    };
    struct CompiledTextureAccess : public CompiledResourceAccess
    {
        TextureResource* texture;
        vk::ImageLayout  layout;
    };
    struct CompiledPass
    {
        RenderPass*                           pass;
        u32                                   id;
        std::vector<vk::BufferMemoryBarrier2> bufferBarriers;
        std::vector<vk::ImageMemoryBarrier2>  imageBarriers;
        std::unordered_set<u32>               passWaitIds;
        vk::CommandBuffer                     cmd;
    };

    RenderGraph(const ILogicalDevice& logicalDevice) : m_logicalDevice(logicalDevice) {}

    RenderPass* AddPass(std::string name, std::vector<RenderPass*> dependencies = {},
                        std::function<void(const IRenderSystem::RenderData&)> exec = nullptr)
    {
        RenderPass::Info info;
        info.name = name;
        info.id = static_cast<u32>(m_passes.size());
        info.dependencies = dependencies;
        std::unique_ptr<RenderPass> pass = std::make_unique<RenderPass>(info);
        pass->exec = std::move(exec);

        m_passes.push_back(std::move(pass));
        return m_passes.back().get();
    };

    u32 GetPassCount() { return static_cast<u32>(m_passes.size()); };
    u32 GetCompiledPassCount() { return static_cast<u32>(m_compiledPasses.size()); };

    void Compile();
    void Execute(const IRenderSystem::RenderData& renderData);

    const ILogicalDevice& m_logicalDevice;

    std::vector<std::unique_ptr<RenderPass>> m_passes;

    std::vector<CompiledPass>          m_compiledPasses;
    std::vector<CompiledBufferAccess>  m_compiledBufferAccesses;
    std::vector<CompiledTextureAccess> m_compiledTextureAccesses;

private:
    std::vector<RenderPass*> TopologicalSort(const std::vector<std::unique_ptr<RenderPass>>& passes);
};
} // namespace Humongous
