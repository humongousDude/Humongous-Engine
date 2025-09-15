#include "frame_scheduler.hpp"
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
        m_timelineValues[i] = 0;
        m_workPackets[i].reserve(128);
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

void WorkScheduler::BeginFrame()
{
    // m_timelineValues[m_currentFrameIndex] = 0;
    m_workPackets[m_currentFrameIndex].clear();
}

void WorkScheduler::AddWork(vk::CommandBuffer cmd, vk::Queue queue)
{
    WorkPacket packet;
    packet.commandBuffer = cmd;
    packet.queue = queue;
    packet.waitValue = m_timelineValues[m_currentFrameIndex];
    packet.signalValue = m_timelineValues[m_currentFrameIndex] + 1;

    m_workPackets[m_currentFrameIndex].push_back(packet);
    m_timelineValues[m_currentFrameIndex]++;
}

void WorkScheduler::Flush(vk::Fence fence, vk::Semaphore imageAvailableSemaphore, vk::Semaphore renderFinishedSemaphore)
{
    std::vector<vk::SubmitInfo2> submits;

    for(u32 i = 0; i < m_workPackets[m_currentFrameIndex].size(); ++i)
    {
        const auto& work = m_workPackets[m_currentFrameIndex][i];

        vk::SubmitInfo2 submit{};

        vk::CommandBufferSubmitInfo cmdInfo(work.commandBuffer);
        submit.setCommandBufferInfos(cmdInfo);

        std::vector<vk::SemaphoreSubmitInfo> waitSemaphores;
        std::vector<vk::SemaphoreSubmitInfo> signalSemaphores;

        if(i == 0 && imageAvailableSemaphore)
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

        if(i == m_workPackets[m_currentFrameIndex].size() - 1)
        {
            vk::SemaphoreSubmitInfo renderFinishedSignal{};
            renderFinishedSignal.semaphore = renderFinishedSemaphore;
            renderFinishedSignal.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
            signalSemaphores.push_back(renderFinishedSignal);
        }

        submit.setWaitSemaphoreInfos(waitSemaphores);
        submit.setSignalSemaphoreInfos(signalSemaphores);

        vk::Result result;
        vk::Fence  submitFence = (i == m_workPackets[m_currentFrameIndex].size() - 1) ? fence : VK_NULL_HANDLE;
        result = work.queue.submit2(1, &submit, submitFence);

        if(result != vk::Result::eSuccess)
        {
            HGERROR("Failed to submit command buffer with wait/signal values of %i, %i", work.waitValue, work.signalValue);
        }
    }

    m_currentFrameIndex = (m_currentFrameIndex + 1) % static_cast<u32>(Globals::Limits::MaxFramesInFlight);
}

} // namespace Humongous
