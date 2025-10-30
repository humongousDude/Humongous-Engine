#pragma once

// std lib
#include <memory>

#include "abstractions/descriptor_layout.hpp"
#include "abstractions/descriptor_pool.hpp"
#include "logger.hpp"
#include "singleton.hpp"
#include "ui/widget.hpp"

namespace Humongous
{
class UI : public Singleton<UI>
{
public:
    struct UICreationInfo
    {
        const class IInstance&      instance;
        const class ILogicalDevice& logicalDevice;
        const Window&               window;
    };

    static void Init(class IInstance& instance, const class ILogicalDevice& logicalDevice, const Window& window)
    {
        Get().Internal_Init(instance, logicalDevice, window);
    }
    static void Shutdown() { Get().Internal_Shutdown(); }
    static void BeginUIFrame() { Get().Internal_BeginUIFrame(); }
    static void EndUIFrame(vk::CommandBuffer cmd) { Get().Internal_EndUIFrame(cmd); }

    static void Debug_DrawMetrics(const s16& draws, const Eigen::Vector3f& camPosition) { Get().Internal_Debug_DrawMetrics(draws, camPosition); }
    static void Debug_DrawObjectData(std::unordered_map<u32, class Entity>& objects) { Get().Internal_Debug_DrawObjectData(objects); }

    static void                   DrawWidgetList() { Get().Internal_DrawWidgetList(); }
    static std::vector<UiWidget*> GetWidgetList() { return Get().Internal_GetWidgetList(); }
    static void                   AddWidgetToList(UiWidget* widg) { Get().Internal_AddWidgetToList(widg); }
    static void                   PopWidgetAtIndex(const u32 index) { Get().Internal_PopWidgetAtIndex(index); }

    static void RenderViewport() { Get().Internal_RenderViewport(); };
    static void RecreateViewportResources(const class Image& sceneImage, const u32& index)
    {
        Get().Internal_RecreateViewportResources(sceneImage, index);
    };

    static vk::Extent2D GetViewportSize() { return vk::Extent2D(Get().m_viewportWidth, Get().m_viewportHeight); }
    static vk::Extent2D GetViewportSizePixels()
    {
        ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;

        return {static_cast<u32>(Get().m_viewportWidth * scale.x), static_cast<u32>(Get().m_viewportHeight * scale.y)};
    }

private:
    b8  m_hasInitialized{false};
    b8  m_startedFrame{false};
    u32 m_viewportWidth{800};
    u32 m_viewportHeight{600};
    u32 m_currentFrameIndex{0};

    // ImGui stores the texture ID as a descriptor set when using Vulkan
    std::array<vk::DescriptorSet, static_cast<u32>(Globals::Limits::MaxFramesInFlight)> m_sceneTextureID{};
    std::array<ImTextureRef, static_cast<u32>(Globals::Limits::MaxFramesInFlight)>      m_sceneTextureRef{};

    const ILogicalDevice* m_logicalDevice;

    std::unique_ptr<DescriptorPool>      m_pool;
    std::unique_ptr<DescriptorSetLayout> m_setLayout;

    std::vector<UiWidget*> m_widgets;

    vk::PipelineRenderingCreateInfo m_renderingInfo = {};

    void InitDescriptorThings();

    void Internal_Init(const class IInstance& instance, const ILogicalDevice& logicalDevice, const Window& window);
    void Internal_Shutdown();
    void Internal_BeginUIFrame();
    void Internal_EndUIFrame(vk::CommandBuffer cmd);

    void Internal_DrawWidgetList();

    void Internal_RenderViewport();
    void Internal_RecreateViewportResources(const class Image& sceneImage, const u32& index);

    void                   Internal_AddWidgetToList(UiWidget* widget);
    void                   Internal_PopWidgetAtIndex(u32 index);
    std::vector<UiWidget*> Internal_GetWidgetList();

    void Internal_Debug_DrawObjectData(std::unordered_map<u32, class Entity>& objects);
    void Internal_Debug_DrawMetrics(const s16& draws, const Eigen::Vector3f& camPosition);
};
}; // namespace Humongous
