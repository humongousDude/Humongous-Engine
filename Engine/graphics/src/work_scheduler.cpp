#include "work_scheduler.hpp"
#include "abstractions/buffer.hpp"
#include "logger.hpp"
#include "logical_device.hpp"

namespace Humongous
{

WorkScheduler::WorkScheduler(const ILogicalDevice& logicalDevice) : m_logicalDevice{logicalDevice}
{
    vk::SemaphoreCreateInfo     createInfo{};
    vk::SemaphoreTypeCreateInfo typeCreateInfo{};
    typeCreateInfo.semaphoreType = vk::SemaphoreType::eTimeline;
    typeCreateInfo.initialValue = 0;
    createInfo.pNext = &typeCreateInfo;

    for(u32 i = 0; i < static_cast<u32>(Globals::Limits::MaxFramesInFlight); ++i)
    {
        m_timelineValues[i] = 1;
        m_graphicsPackets[i].reserve(128);
        m_computePackets[i].reserve(128);
        m_transferPackets[i].reserve(128);
        m_semaphores[i] = m_logicalDevice.GetVkDevice().createSemaphore(createInfo);
    }
}

WorkScheduler::~WorkScheduler()
{
    for(u32 i = 0; i < static_cast<u32>(Globals::Limits::MaxFramesInFlight); ++i)
    {
        m_logicalDevice.GetVkDevice().destroySemaphore(m_semaphores[i]);
    }
}

WorkScheduler::WorkPacketHandle WorkScheduler::AddWork(vk::CommandBuffer cmd, vk::Queue queue, const std::vector<WorkPacketHandle>& waits)
{
    WorkPacket packet;
    packet.commandBuffer = cmd;
    packet.queue = queue;

    u64 waitValue = 0;

    if(!waits.empty())
    {
        for(const auto& wait: waits)
        {
            if(wait.signalValue > waitValue && wait.signalValue != INVALID_WORK_SIGNAL) { waitValue = wait.signalValue; }
        }
    }
    else
    {
        waitValue = 0;
    }

    packet.waitValue = waitValue;
    packet.signalValue = m_timelineValues[m_currentFrameIndex] + 1;

    m_timelineValues[m_currentFrameIndex]++;

    if(queue == m_logicalDevice.GetGraphicsQueue()) { m_graphicsPackets[m_currentFrameIndex].push_back(packet); }
    else if(queue == m_logicalDevice.GetComputeQueue()) { m_computePackets[m_currentFrameIndex].push_back(packet); }
    else if(queue == m_logicalDevice.GetTransferQueue()) { m_transferPackets[m_currentFrameIndex].push_back(packet); }
    else
    {
        HGERROR("Attempted to schedule work on an unknown queue");
        return {};
    }

    WorkPacketHandle handle;
    handle.signalValue = packet.signalValue;
    return handle;
}

void WorkScheduler::AddStagingBuffers(std::vector<std::unique_ptr<Buffer>>& stagingBuffers)
{
    m_buffersToDestroy[m_currentFrameIndex].insert(m_buffersToDestroy[m_currentFrameIndex].end(), std::make_move_iterator(stagingBuffers.begin()),
                                                   std::make_move_iterator(stagingBuffers.end()));
    stagingBuffers.clear();
}

void WorkScheduler::CollectGarbage()
{
    while(!m_stagingBufferGraveyards[m_currentFrameIndex].empty())
    {
        if(m_stagingBufferGraveyards[m_currentFrameIndex].front().signalValue < m_timelineValues[m_currentFrameIndex])
        {
            m_stagingBufferGraveyards[m_currentFrameIndex].pop_front();
        }
        else
        {
            break;
        }
    }
}

void WorkScheduler::Flush(vk::Fence fence, vk::Semaphore imageAvailableSemaphore, vk::Semaphore renderFinishedSemaphore)
{
    std::vector<vk::SubmitInfo2> submits;

    for(u32 i = 0; i < m_graphicsPackets[m_currentFrameIndex].size(); ++i)
    {
        const auto& work = m_graphicsPackets[m_currentFrameIndex][i];

        vk::SubmitInfo2 submit{};

        vk::CommandBufferSubmitInfo cmdInfo(work.commandBuffer);
        submit.setCommandBufferInfos(cmdInfo);

        std::vector<vk::SemaphoreSubmitInfo> waitSemaphores;
        std::vector<vk::SemaphoreSubmitInfo> signalSemaphores;

        if(i == 0 && imageAvailableSemaphore != VK_NULL_HANDLE)
        {
            vk::SemaphoreSubmitInfo availableWait{};
            availableWait.semaphore = imageAvailableSemaphore;
            availableWait.stageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
            waitSemaphores.push_back(availableWait);
        }

        vk::SemaphoreSubmitInfo timelineWait{};
        timelineWait.semaphore = m_semaphores[m_currentFrameIndex];
        timelineWait.value = work.waitValue;
        timelineWait.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
        waitSemaphores.push_back(timelineWait);

        vk::SemaphoreSubmitInfo timelineSignal{};
        timelineSignal.semaphore = m_semaphores[m_currentFrameIndex];
        timelineSignal.value = work.signalValue;
        timelineSignal.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
        signalSemaphores.push_back(timelineSignal);

        if(i == m_graphicsPackets[m_currentFrameIndex].size() - 1 && renderFinishedSemaphore != VK_NULL_HANDLE)
        {
            vk::SemaphoreSubmitInfo renderFinishedSignal{};
            renderFinishedSignal.semaphore = renderFinishedSemaphore;
            renderFinishedSignal.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
            signalSemaphores.push_back(renderFinishedSignal);
        }

        submit.setWaitSemaphoreInfos(waitSemaphores);
        submit.setSignalSemaphoreInfos(signalSemaphores);

        vk::Result result;
        vk::Fence  submitFence = (i == m_graphicsPackets[m_currentFrameIndex].size() - 1) ? fence : VK_NULL_HANDLE;
        result = work.queue.submit2(1, &submit, submitFence);

        if(result != vk::Result::eSuccess)
        {
            HGERROR("Failed to submit command buffer with wait/signal values of %i, %i", work.waitValue, work.signalValue);
        }
    }

    auto submitGenericPackets = [&](const std::vector<WorkPacket>& packets, const char* queueName) {
        for(const auto& work: packets)
        {
            vk::SubmitInfo2 submit{};

            // Command Buffer Info
            vk::CommandBufferSubmitInfo cmdInfo(work.commandBuffer);
            submit.setCommandBufferInfos(cmdInfo);

            std::vector<vk::SemaphoreSubmitInfo> waitSemaphores;
            std::vector<vk::SemaphoreSubmitInfo> signalSemaphores;

            // Wait on Timeline Semaphore
            vk::SemaphoreSubmitInfo timelineWait{};
            timelineWait.semaphore = m_semaphores[m_currentFrameIndex];
            timelineWait.value = work.waitValue;
            timelineWait.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
            waitSemaphores.push_back(timelineWait);

            // Signal Timeline Semaphore
            vk::SemaphoreSubmitInfo timelineSignal{};
            timelineSignal.semaphore = m_semaphores[m_currentFrameIndex];
            timelineSignal.value = work.signalValue;
            timelineSignal.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
            signalSemaphores.push_back(timelineSignal);

            submit.setWaitSemaphoreInfos(waitSemaphores);
            submit.setSignalSemaphoreInfos(signalSemaphores);

            // No external fence or swapchain semaphores for generic packets
            vk::Result result = work.queue.submit2(1, &submit, VK_NULL_HANDLE);

            if(result != vk::Result::eSuccess)
            {
                HGERROR("Failed to submit %s command buffer with wait/signal values of %i, %i", queueName, work.waitValue, work.signalValue);
            }
        }
    };

    submitGenericPackets(m_computePackets[m_currentFrameIndex], "COMPUTE");
    submitGenericPackets(m_transferPackets[m_currentFrameIndex], "TRANSFER");

    u64 finalSignalValue = m_timelineValues[m_currentFrameIndex];

    for(auto& buf: m_buffersToDestroy[m_currentFrameIndex])
    {
        StagingBufferGrave grave;
        grave.buffer = std::move(buf);
        grave.signalValue = finalSignalValue;
        m_stagingBufferGraveyards[m_currentFrameIndex].push_back(std::move(grave));
    }

    // m_buffersToDestroy[m_currentFrameIndex].clear();
    m_graphicsPackets[m_currentFrameIndex].clear();
    m_computePackets[m_currentFrameIndex].clear();
    m_transferPackets[m_currentFrameIndex].clear();

    m_currentFrameIndex = (m_currentFrameIndex + 1) % static_cast<u32>(Globals::Limits::MaxFramesInFlight);
}

} // namespace Humongous
