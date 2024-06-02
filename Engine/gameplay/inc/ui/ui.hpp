#pragma once

// std lib
#include <memory>

#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool.hpp"
#include "instance.hpp"
#include "logical_device.hpp"
#include "render_pipeline.hpp"
#include "renderer.hpp"
#include "singleton.hpp"

namespace Humongous
{
class UI : public Singleton<UI>
{
public:
    struct UICreationInfo
    {
        Humongous::Instance* instance;
        LogicalDevice*       logicalevice;
        Window*              window;
        Renderer*            renderer;
    };

    static void Init(class Instance* instance, LogicalDevice* logicalDevice, Window* window)
    {
        Get().Internal_Init(instance, logicalDevice, window);
    }
    static void Shutdown() { Get().Internal_Shutdown(); }

    static void BeginUIFrame(vk::CommandBuffer cmd) { Get().Internal_BeginUIFrame(cmd); }
    static void EndUIFRame(vk::CommandBuffer cmd) { Get().Internal_EndUIFRame(cmd); }
    static void Debug_DrawMetrics() { Get().Internal_Debug_DrawMetrics(); }

private:
    bool m_hasInited{false};
    bool m_initedFrame{false};

    LogicalDevice*                  m_logicalDevice{nullptr};
    vk::PipelineLayout              m_pipelineLayout{VK_NULL_HANDLE};
    std::unique_ptr<RenderPipeline> m_renderPipeline{nullptr};

    std::unique_ptr<DescriptorPool>      m_pool;
    std::unique_ptr<DescriptorSetLayout> m_setLayout;

    VkPipelineRenderingCreateInfo m_renderingInfo;

    void InitDescriptorThings();
    void InitPipeline();

    void Internal_Init(class Instance* instance, LogicalDevice* logicalDevice, Window* window);
    void Internal_Shutdown();
    void Internal_BeginUIFrame(vk::CommandBuffer cmd);
    void Internal_EndUIFRame(vk::CommandBuffer cmd);
    void Internal_Debug_DrawMetrics();
};
}; // namespace Humongous
