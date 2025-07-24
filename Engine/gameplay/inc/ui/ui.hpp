#pragma once

// std lib
#include <memory>

#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool.hpp"
#include "instance.hpp"
#include "logical_device.hpp"
#include "renderer.hpp"
#include "singleton.hpp"
#include "ui/widget.hpp"

namespace Humongous
{
class UI : public Singleton<UI>
{
public:
    struct UICreationInfo
    {
        // Apparently we can't just have "instance", we need "class Instance" for some reason.
        const class Instance& instance;
        const LogicalDevice&  logicalDevice;
        const Window&         window;
        const Renderer&       renderer;
    };

    static void Init(class Instance& instance, const LogicalDevice& logicalDevice, const Window& window)
    {
        Get().Internal_Init(instance, logicalDevice, window);
    }
    static void Shutdown() { Get().Internal_Shutdown(); }
    static void BeginUIFrame(vk::CommandBuffer cmd) { Get().Internal_BeginUIFrame(cmd); }
    static void EndUIFrame(vk::CommandBuffer cmd) { Get().Internal_EndUIFrame(cmd); }

    static void Debug_DrawMetrics(const s16& draws, const Eigen::Vector3f& camPosition) { Get().Internal_Debug_DrawMetrics(draws, camPosition); }
    static void Debug_DrawObjectData(std::unordered_map<n32, class Entity>& objects) { Get().Internal_Debug_DrawObjectData(objects); }

    static void                   DrawWidgetList(const vk::CommandBuffer cmd) { Get().Internal_DrawWidgetList(); }
    static std::vector<UiWidget*> GetWidgetList() { return Get().Internal_GetWidgetList(); }
    static void                   AddWidgetToList(UiWidget* widg) { Get().Internal_AddWidgetToList(widg); }
    static void                   PopWidgetAtIndex(const n32 index) { Get().Internal_PopWidgetAtIndex(index); }

private:
    bool m_hasInitialized{false};
    bool m_startedFrame{false};

    const LogicalDevice* m_logicalDevice;

    std::unique_ptr<DescriptorPool>      m_pool;
    std::unique_ptr<DescriptorSetLayout> m_setLayout;

    std::vector<UiWidget*> m_widgets;

    vk::PipelineRenderingCreateInfo m_renderingInfo = {};

    void InitDescriptorThings();

    void Internal_Init(const class Instance& instance, const LogicalDevice& logicalDevice, const Window& window);
    void Internal_Shutdown();
    void Internal_BeginUIFrame(vk::CommandBuffer cmd);
    void Internal_EndUIFrame(vk::CommandBuffer cmd);

    void Internal_DrawWidgetList();

    void                   Internal_AddWidgetToList(UiWidget* widget);
    void                   Internal_PopWidgetAtIndex(n32 index);
    std::vector<UiWidget*> Internal_GetWidgetList();

    void Internal_Debug_DrawObjectData(std::unordered_map<n32, class Entity>& objects);
    void Internal_Debug_DrawMetrics(const s16& draws, const Eigen::Vector3f& camPosition);
};
}; // namespace Humongous
