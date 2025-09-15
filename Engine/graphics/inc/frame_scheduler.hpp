#pragma once

#include "defines.hpp"
#include "globals.hpp"
#include "non_copyable.hpp"
#include <vulkan/vulkan.hpp>

namespace Humongous
{

class WorkScheduler : NonCopyable
{

public:
    WorkScheduler(const class ILogicalDevice& logicalDevice);
    ~WorkScheduler();

    void BeginFrame();
    void AddWork(vk::CommandBuffer cmd, vk::Queue queue);
    void Flush(vk::Fence fence, vk::Semaphore imageAvailableSemaphore, vk::Semaphore renderFinishedSemaphore);

private:
    struct WorkPacket
    {
        vk::CommandBuffer commandBuffer;
        vk::Queue         queue;
        u64               waitValue;
        u64               signalValue;
    };

    const class ILogicalDevice& m_logicalDevice;

    std::array<std::vector<WorkPacket>, static_cast<u32>(Globals::Limits::MaxFramesInFlight)> m_workPackets;
    std::array<u64, static_cast<u32>(Globals::Limits::MaxFramesInFlight)>                     m_timelineValues;
    std::array<vk::Semaphore, static_cast<u32>(Globals::Limits::MaxFramesInFlight)>           m_semaphores;
    u32                                                                                       m_currentFrameIndex{0};
};

} // namespace Humongous
