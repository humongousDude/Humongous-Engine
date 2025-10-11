#include "render_graph.hpp"
#include "logger.hpp"
#include <queue>
#include <unordered_map>

namespace Humongous
{

vk::Queue QueueTypeToQueue(const ILogicalDevice& dev, const RenderResource::Queue& q)
{
    switch(q)
    {
        case RenderResource::Queue::Graphics:
            return dev.GetGraphicsQueue();
        case RenderResource::Queue::Compute:
            return dev.GetComputeQueue();
        case RenderResource::Queue::Transfer:
            return dev.GetTransferQueue();
        default:
            HGERROR("Unknown queue type");
            return dev.GetGraphicsQueue();
    }
}

vk::Queue QueueTypeToQueue(const ILogicalDevice& dev, const RenderPass::Queue& q)
{
    switch(q)
    {
        case RenderPass::Queue::Graphics:
            return dev.GetGraphicsQueue();
        case RenderPass::Queue::Compute:
            return dev.GetComputeQueue();
        case RenderPass::Queue::Transfer:
            return dev.GetTransferQueue();
        default:
            HGERROR("Unknown queue type");
            return dev.GetGraphicsQueue();
    }
}

std::vector<RenderPass*> RenderGraph::TopologicalSort(const std::vector<std::unique_ptr<RenderPass>>& passes)
{
    std::map<const RenderPass*, int> inDegree;
    std::queue<const RenderPass*>    q;
    std::vector<RenderPass*>         sortedPasses;

    for(const auto& pass: passes) { inDegree[pass.get()] = 0; }
    for(const auto& pass: passes)
    {
        for(RenderPass* dependencyPass: pass->GetInfo().dependencies)
        {
            if(inDegree.count(dependencyPass)) { inDegree[dependencyPass]++; }
            else
            {
                HGERROR("External or unmanaged dependency detected! The render graph does not support this!");
            }
        }
    }
    for(const auto& pair: inDegree)
    {
        if(pair.second == 0) { q.push(pair.first); }
    }

    while(!q.empty())
    {
        const RenderPass* u = q.front();
        q.pop();

        sortedPasses.push_back(const_cast<RenderPass*>(u));
        std::map<const RenderPass*, std::vector<const RenderPass*>> adj;
        std::map<const RenderPass*, int>                            inDegree;

        for(const auto& pass: passes)
        {
            inDegree[pass.get()] = 0;
            adj[pass.get()] = {};
        }

        for(const auto& v: passes)
        {
            for(RenderPass* u_ptr: v->GetInfo().dependencies)
            {
                const RenderPass* u = u_ptr;
                if(inDegree.count(u))
                {
                    adj[u].push_back(v.get());
                    inDegree[v.get()]++;
                }
            }
        }

        std::queue<const RenderPass*> q;
        for(const auto& pair: inDegree)
        {
            if(pair.second == 0) { q.push(pair.first); }
        }

        std::vector<RenderPass*> sortedPasses;
        while(!q.empty())
        {
            const RenderPass* u = q.front();
            q.pop();

            sortedPasses.push_back(const_cast<RenderPass*>(u));

            for(const RenderPass* v: adj[u])
            {
                inDegree[v]--;
                if(inDegree[v] == 0) { q.push(v); }
            }
        }

        if(sortedPasses.size() != passes.size()) { throw std::runtime_error("Render Graph has a cycle! Cannot topologically sort."); }

        return sortedPasses;
    }
    if(sortedPasses.size() != passes.size()) { HGERROR("RenderGraph sort failed! It's likely cyclical!"); }
    return sortedPasses;
}

void RenderGraph::Compile()
{
    auto ordererdPasses = TopologicalSort(m_passes); // TODO: Actual topological sort
    m_compiledPasses.clear();

    std::unordered_map<RenderPass*, u32>                        passHandleMap;
    std::unordered_map<RenderResource*, u32>                    lastWriterHandles;
    std::unordered_map<RenderResource*, RenderResource::Queue>  lastWriterQueues;
    std::unordered_map<BufferResource*, CompiledBufferAccess>   bufferAccessMap;
    std::unordered_map<TextureResource*, CompiledTextureAccess> textureAccessMap;

    for(auto pass: ordererdPasses)
    {
        CompiledPass compiledPass;
        compiledPass.pass = pass;
        compiledPass.id = static_cast<u32>(m_compiledPasses.size());

        RenderResource::Queue passQueue = RenderResource::Queue::Graphics;
        vk::Queue             queue = QueueTypeToQueue(m_logicalDevice, pass->m_info.queue);

        std::unordered_set<u32> waits;

        for(RenderPass* dep: pass->GetInfo().dependencies)
        {
            if(passHandleMap.count(dep)) { waits.insert(passHandleMap[dep]); }
        }

        for(const auto& accessed: pass->genericTexture)
        {
            RenderResource* r = accessed.texture;
            auto            it = lastWriterHandles.find(r);
            if(it != lastWriterHandles.end()) { waits.insert(it->second); }
        }
        for(const auto& b: pass->genericBuffer)
        {
            RenderResource* r = b.buffer;
            auto            it = lastWriterHandles.find(r);
            if(it != lastWriterHandles.end()) { waits.insert(it->second); }
        }

        compiledPass.passWaitIds = std::move(waits);

        for(auto& access: pass->genericTexture)
        {
            Image&          img = access.texture->GetTexture()->GetAllocatedImage();
            vk::ImageLayout desiredLayout = access.layout;

            CompiledTextureAccess previousState;
            if(textureAccessMap.find(access.texture) != textureAccessMap.end()) { previousState = textureAccessMap[access.texture]; }
            else
            {
                previousState.layout = img.GetLayout();
                // previousState.queue = passQueue;
                previousState.stages = {};
                previousState.access = {};
            }

            b8 needsTransition = (previousState.layout != desiredLayout) || (lastWriterQueues[access.texture] != passQueue);

            if(!needsTransition)
            {
                CompiledTextureAccess existingAccess;
                existingAccess.passId = compiledPass.id;
                existingAccess.access = access.access;
                existingAccess.stages = access.stages;
                existingAccess.layout = access.layout;
                existingAccess.texture = access.texture;

                textureAccessMap[access.texture] = existingAccess;
                continue;
            }
            vk::ImageMemoryBarrier2 barrier{};
            barrier.srcStageMask = previousState.stages ? previousState.stages : vk::PipelineStageFlagBits2::eTopOfPipe;
            barrier.srcAccessMask = previousState.access;
            barrier.dstStageMask = access.stages ? access.stages : vk::PipelineStageFlagBits2::eBottomOfPipe;
            barrier.dstAccessMask = access.access;
            barrier.oldLayout = previousState.layout;
            barrier.newLayout = access.layout;
            barrier.image = img.GetImage();
            barrier.subresourceRange.aspectMask = img.GetAspectFlags();
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = img.GetMipLevels();
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = img.GetArrayLayerCount();

            // ownership transfer? set queue family indices accordingly.
            // ADAPT: convert your RenderResource::Queue to actual queue family indices if required.
            // if(previousState.queue != passQueue)
            // {
            //     barrier.srcQueueFamilyIndex = static_cast<uint32_t>(previousState.queue); // ADAPT: use your family index mapping
            //     barrier.dstQueueFamilyIndex = static_cast<uint32_t>(passQueue);           // ADAPT: use your family index mapping
            // }
            // else
            // {
            //     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            //     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            // }

            compiledPass.imageBarriers.push_back(barrier);

            // update compiled resource state to new
            CompiledTextureAccess newAccess;
            newAccess.passId = compiledPass.id;
            newAccess.access = access.access;
            newAccess.stages = access.stages;
            newAccess.layout = access.layout;
            newAccess.texture = access.texture;
            textureAccessMap[access.texture] = newAccess;
        }
        // update lastWriter maps for resources this pass wrote
        for(const auto& w: pass->genericTexture)
        {
            lastWriterHandles[w.texture] = compiledPass.id;
            lastWriterQueues[w.texture] = passQueue;
        }
        for(const auto& b: pass->genericBuffer)
        {
            lastWriterHandles[b.buffer] = compiledPass.id;
            lastWriterQueues[b.buffer] = passQueue;
        }

        m_compiledPasses.push_back(compiledPass);
    }
}

void RenderGraph::Execute(const IRenderSystem::RenderData& renderData)
{
    for(auto& pass: m_compiledPasses)
    {
        auto cmd = m_logicalDevice.BeginSingleTimeCommands();
        pass.cmd = cmd;

        if(!pass.imageBarriers.empty())
        {
            vk::DependencyInfo depInfo{};
            depInfo.imageMemoryBarrierCount = static_cast<u32>(pass.imageBarriers.size());
            depInfo.pImageMemoryBarriers = pass.imageBarriers.data();
            cmd.pipelineBarrier2(depInfo);
        }
        if(pass.pass->exec) { pass.pass->exec(renderData); }
    }
    m_passes.clear();
}

} // namespace Humongous
