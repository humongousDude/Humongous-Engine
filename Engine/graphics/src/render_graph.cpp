#include "render_graph.hpp"
#include "logger.hpp"

namespace Humongous
{
ImageHandle RenderGraphPass::AddTransientImage(RenderGraphRegistry& registry, const Utils::AllocatedImageCreateInfo& imageCreateInfo,
                                               const ResourceUsage& usage)
{
    ImageIdentity imageIdentity;
    imageIdentity.width = imageCreateInfo.width;
    imageIdentity.height = imageCreateInfo.height;
    imageIdentity.mipLevels = imageCreateInfo.mipLevels;
    imageIdentity.layout = imageCreateInfo.initialLayout;

    auto handle = registry.RegisterTransientImage(imageIdentity);

    if(usage == ResourceUsage::Read) { m_readsFromImage.push_back(handle); }
    else if(usage == ResourceUsage::Write) { m_writesToImage.push_back(handle); }
    else if(usage == ResourceUsage::ReadWrite)
    {
        m_readsFromImage.push_back(handle);
        m_writesToImage.push_back(handle);
    }

    return handle;
}

BufferHandle RenderGraphPass::AddTransientBuffer(RenderGraphRegistry& registry, const Buffer::BufferCreateInfo& bufferCreateInfo,
                                                 const ResourceUsage& usage)
{
    BufferIdentity bufferIdentity;
    bufferIdentity.size = bufferCreateInfo.size;
    bufferIdentity.usage = bufferCreateInfo.bufferUsage;
    bufferIdentity.memoryProperty = bufferCreateInfo.properties;

    auto handle = registry.RegisterTransientBuffer(bufferIdentity);

    if(usage == ResourceUsage::Read) { m_readsFromBuffer.push_back(handle); }
    else if(usage == ResourceUsage::Write) { m_writesToBuffer.push_back(handle); }
    else if(usage == ResourceUsage::ReadWrite)
    {
        m_readsFromBuffer.push_back(handle);
        m_writesToBuffer.push_back(handle);
    }

    return handle;
}

ImageHandle RenderGraphPass::ImportImage(RenderGraphRegistry& registry, AllocatedImage* image, const vk::ImageLayout& initialLayout,
                                         const ResourceUsage& usage)
{
    ImageIdentity imageIdentity;
    imageIdentity.width = image->width;
    imageIdentity.height = image->height;
    imageIdentity.mipLevels = image->mipLevels;
    imageIdentity.layout = image->imageLayout;

    auto handle = registry.RegisterImportedImage(image, imageIdentity);

    if(usage == ResourceUsage::Read) { m_readsFromImage.push_back(handle); }
    else if(usage == ResourceUsage::Write) { m_writesToImage.push_back(handle); }
    else if(usage == ResourceUsage::ReadWrite)
    {
        m_readsFromImage.push_back(handle);
        m_writesToImage.push_back(handle);
    }

    return handle;
}

BufferHandle RenderGraphPass::ImportBuffer(RenderGraphRegistry& registry, Buffer* buffer, const ResourceUsage& usage)
{
    BufferIdentity bufferIdentity;
    bufferIdentity.size = buffer->GetBufferSize();
    bufferIdentity.usage = buffer->GetUsageFlags();
    bufferIdentity.memoryProperty = buffer->GetMemoryPropertyFlags();

    auto handle = registry.RegisterImportedBuffer(buffer, bufferIdentity);

    if(usage == ResourceUsage::Read) { m_readsFromBuffer.push_back(handle); }
    else if(usage == ResourceUsage::Write) { m_writesToBuffer.push_back(handle); }
    else if(usage == ResourceUsage::ReadWrite)
    {
        m_readsFromBuffer.push_back(handle);
        m_writesToBuffer.push_back(handle);
    }

    return handle;
}

void RenderGraphPass::SetExecuteCallback(std::function<void(vk::CommandBuffer, const RenderGraphRegistry&)>&& execute)
{
    m_executeCallback = std::move(execute);
}

RenderGraphRegistry::RenderGraphRegistry(const ILogicalDevice& logicalDevice) : m_logicalDevice{logicalDevice} {}

RenderGraphRegistry::~RenderGraphRegistry() {}

const ImageResource& RenderGraphRegistry::GetImageResource(ImageResource handle) { return m_imageResources.at(handle.id); }

const BufferResource& RenderGraphRegistry::GetBufferResource(BufferResource handle) { return m_bufferResources.at(handle.id); }

ImageHandle RenderGraphRegistry::RegisterTransientImage(const ImageIdentity& identity)
{
    ImageResource imageResource;
    imageResource.id = m_nextImageId++;
    imageResource.isTransient = true;
    imageResource.imageCreateInfo = nullptr;
    imageResource.externalImage = nullptr;

    imageResource.firstAccessPassId = INVALID_ID;
    imageResource.lastAccessPassId = 0;

    imageResource.currentLayout = vk::ImageLayout::eUndefined;
    imageResource.requiredLayout = vk::ImageLayout::eUndefined;

    m_imageResources.insert({imageResource.id, imageResource});

    return {imageResource.id};
}

BufferHandle RenderGraphRegistry::RegisterTransientBuffer(const BufferIdentity& identity)
{
    BufferResource bufferResource;
    bufferResource.id = m_nextBufferId++;
    bufferResource.isTransient = true;
    bufferResource.bufferCreateInfo = nullptr;
    bufferResource.externalBuffer = nullptr;

    bufferResource.firstAccessPassId = INVALID_ID;
    bufferResource.lastAccessPassId = 0;

    m_bufferResources.insert({bufferResource.id, bufferResource});

    return {bufferResource.id};
}

ImageHandle RenderGraphRegistry::RegisterImportedImage(AllocatedImage* image, const ImageIdentity& identity)
{
    ImageResource imageResource;
    imageResource.id = m_nextImageId++;
    imageResource.isTransient = false;
    imageResource.imageCreateInfo = nullptr;
    imageResource.externalImage = image;

    imageResource.firstAccessPassId = INVALID_ID;
    imageResource.lastAccessPassId = 0;

    imageResource.currentLayout = image->imageLayout;
    imageResource.requiredLayout = image->imageLayout;

    m_imageResources.insert({imageResource.id, imageResource});

    return {imageResource.id};
}

BufferHandle RenderGraphRegistry::RegisterImportedBuffer(Buffer* buffer, const BufferIdentity& identity)
{
    BufferResource bufferResource;
    bufferResource.id = m_nextBufferId++;
    bufferResource.isTransient = false;
    bufferResource.bufferCreateInfo = nullptr;
    bufferResource.externalBuffer = buffer;

    bufferResource.firstAccessPassId = INVALID_ID;
    bufferResource.lastAccessPassId = 0;

    m_bufferResources.insert({bufferResource.id, bufferResource});

    return {bufferResource.id};
}

void RenderGraphRegistry::Reset()
{
    m_imageResources.clear();
    m_bufferResources.clear();

    m_nextImageId = 0;
    m_nextBufferId = 0;
}

RenderGraph::RenderGraph(const ILogicalDevice& logicalDevice) : m_logicalDevice{logicalDevice}, m_registry{logicalDevice} {}

RenderGraph::~RenderGraph() {}

RenderGraphPass& RenderGraph::AddPass(const std::string& name)
{
    RenderGraphPass pass;
    pass.passId = m_nextPassId;
    pass.name = name;
    m_passes.push_back(pass);

    m_nextPassId++;
    HGINFO("Added pass \" %s \" with id %i", name.c_str(), pass.passId);
    return m_passes[pass.passId];
}

void RenderGraph::Compile()
{
    HGINFO("Compiling %i passes", m_passes.size());

    m_executionOrderPasses.resize(m_passes.size());
    for(auto pass: m_passes)
    {
        HGINFO("Adding pass %s to execution order", pass.name.c_str());
        m_executionOrderPasses[pass.passId] = &pass;
    }

    HGINFO("compiled %i passes", m_executionOrderPasses.size());
}

void RenderGraph::Execute(vk::CommandBuffer commandBuffer)
{
    HGINFO("Executing...");
    for(auto pass: m_executionOrderPasses)
    {
        HGINFO("Executing pass %i", pass->passId);
        if(pass->m_executeCallback) { pass->m_executeCallback(commandBuffer, m_registry); }
    }

    // for(auto pass: m_passes)
    // {
    //     HGINFO("Executing pass %i", pass.passId);
    //     pass.m_executeCallback(commandBuffer, m_registry);
    // }

    HGINFO("done");
}

void RenderGraph::Reset()
{
    HGINFO("Resetting...");
    m_registry.Reset();
    m_executionOrderPasses.clear();
    m_passes.clear();
    m_nextPassId = 0;
    HGINFO("done");
}

} // namespace Humongous
