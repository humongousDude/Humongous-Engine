#pragma once

#include "render_pipeline.hpp"
#include "window.hpp"
#include <deque>
#include <functional>
#include <instance.hpp>
#include <logical_device.hpp>
#include <memory>
#include <physical_device.hpp>
#include <swapchain.hpp>

namespace Humongous
{
struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    void PushDeletor(std::function<void()> deletor) { deletors.push_back(deletor); }

    void Flush()
    {
        for(auto& deletor: deletors) { deletor(); }
        deletors.clear();
    }
};

class VulkanApp
{
public:
    VulkanApp();
    ~VulkanApp();

    void Run();

private:
    DeletionQueue m_mainDeletionQueue;

    std::unique_ptr<Instance>       m_instance;
    std::unique_ptr<Window>         m_window;
    std::unique_ptr<PhysicalDevice> m_physicalDevice;
    std::unique_ptr<LogicalDevice>  m_logicalDevice;
    std::unique_ptr<SwapChain>      m_swapChain;
    std::unique_ptr<RenderPipeline> m_renderPipeline;

    VkPipelineLayout pipelineLayout;

    void Init();
    // TODO: move this function
    void CreatePipelineLayout();
};
} // namespace Humongous
