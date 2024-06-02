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

    void Init(class Instance* instance, LogicalDevice* logicalDevice, Window* window);
    void Shutdown();

    void BeginUIFrame(vk::CommandBuffer cmd);
    void EndUIFRame(vk::CommandBuffer cmd);

    void Debug_DrawMetrics();

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
};
}; // namespace Humongous
