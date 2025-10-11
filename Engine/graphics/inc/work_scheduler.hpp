#pragma once

#include "defines.hpp"
#include "globals.hpp"
#include "memory"
#include "non_copyable.hpp"
#include <deque>
#include <vulkan/vulkan.hpp>

namespace Humongous
{

class WorkScheduler : NonCopyable
{

public:
    static constexpr u64 INVALID_WORK_SIGNAL = std::numeric_limits<u64>::max();

    struct WorkPacketHandle
    {
        u64 signalValue{INVALID_WORK_SIGNAL};
    };

    WorkScheduler(const class ILogicalDevice& logicalDevice);
    ~WorkScheduler();

    WorkPacketHandle AddWork(vk::CommandBuffer cmd, vk::Queue queue, const std::vector<WorkPacketHandle>& waits = {});

    // Note, this function takes ownership of the staging buffers passed in
    void AddStagingBuffers(std::vector<std::unique_ptr<class Buffer>>& stagingBuffers);

    void Flush(vk::Fence fence = VK_NULL_HANDLE, vk::Semaphore imageAvailableSemaphore = VK_NULL_HANDLE,
               vk::Semaphore renderFinishedSemaphore = VK_NULL_HANDLE);

    void CollectGarbage();

private:
    struct WorkPacket
    {
        vk::CommandBuffer commandBuffer;
        vk::Queue         queue;
        u64               waitValue;
        u64               signalValue;
    };

    struct StagingBufferGrave
    {
        std::unique_ptr<class Buffer> buffer;
        u64                           signalValue;
    };

    const class ILogicalDevice& m_logicalDevice;

    std::array<std::vector<WorkPacket>, static_cast<u32>(Globals::Limits::MaxFramesInFlight)> m_graphicsPackets;
    std::array<std::vector<WorkPacket>, static_cast<u32>(Globals::Limits::MaxFramesInFlight)> m_computePackets;
    std::array<std::vector<WorkPacket>, static_cast<u32>(Globals::Limits::MaxFramesInFlight)> m_transferPackets;

    std::array<u64, static_cast<u32>(Globals::Limits::MaxFramesInFlight)>                                        m_timelineValues;
    std::array<vk::Semaphore, static_cast<u32>(Globals::Limits::MaxFramesInFlight)>                              m_semaphores;
    std::array<std::vector<std::unique_ptr<class Buffer>>, static_cast<u32>(Globals::Limits::MaxFramesInFlight)> m_buffersToDestroy;
    u32                                                                                                          m_currentFrameIndex{0};

    std::array<std::deque<StagingBufferGrave>, static_cast<u32>(Globals::Limits::MaxFramesInFlight)> m_stagingBufferGraveyards;
};

} // namespace Humongous
