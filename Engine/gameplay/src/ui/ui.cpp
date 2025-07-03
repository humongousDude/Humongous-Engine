#include "ui/ui.hpp"
#include "logger.hpp"
#include "render_pipeline.hpp"

// lib
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include "ui/widget.hpp"
#include <vulkan/vk_enum_string_helper.h>

namespace Humongous
{

void UI::Internal_Init(const class Instance* instance, LogicalDevice* logicalDevice, const Window* window)
{
    if(m_hasInitialized) { return; }
    HGINFO("Initializing UI...");

    m_logicalDevice = logicalDevice;

    InitDescriptorThings();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForVulkan(window->GetWindow());

    ImGui::StyleColorsDark();

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance->GetVkInstance();
    initInfo.Device = m_logicalDevice->GetVkDevice();

    initInfo.MinImageCount = m_logicalDevice->GetPhysicalDevice()
                                 .QuerySwapChainSupport(m_logicalDevice->GetPhysicalDevice().GetVkPhysicalDevice())
                                 .capabilities.surfaceCapabilities.minImageCount;

    initInfo.ImageCount = m_logicalDevice->GetPhysicalDevice()
                              .QuerySwapChainSupport(m_logicalDevice->GetPhysicalDevice().GetVkPhysicalDevice())
                              .capabilities.surfaceCapabilities.minImageCount +
                          1;
    auto confInfo = RenderPipeline::DefaultPipelineConfigInfo();
    m_renderingInfo = confInfo.renderingInfo;
    m_renderingInfo.depthAttachmentFormat = vk::Format::eUndefined;
    m_renderingInfo.stencilAttachmentFormat = vk::Format::eUndefined;
    m_renderingInfo.colorAttachmentCount = 1;

    initInfo.Queue = m_logicalDevice->GetGraphicsQueue();
    initInfo.QueueFamily = m_logicalDevice->GetGraphicsQueueIndex();
    initInfo.PhysicalDevice = m_logicalDevice->GetPhysicalDevice().GetVkPhysicalDevice();
    initInfo.DescriptorPool = m_pool->GetRawPoolHandle();
    initInfo.UseDynamicRendering = true;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.Subpass = 0;
    initInfo.Allocator = nullptr;
    initInfo.PipelineRenderingCreateInfo = m_renderingInfo;
    initInfo.MinAllocationSize = 1024 * 1024;
    ImGui_ImplVulkan_Init(&initInfo);

    m_hasInitialized = true;
    HGINFO("UI Initialized");
}

void UI::Internal_Shutdown()
{
    if(!m_hasInitialized) { return; }
    HGINFO("Shutting UI down");
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    m_pool.reset();
    m_setLayout.reset();
    HGINFO("Successfully shut UI down");
}

void UI::InitDescriptorThings()
{
    {
        DescriptorPool::Builder builder{*m_logicalDevice};
        builder.AddPoolSize(vk::DescriptorType::eCombinedImageSampler, 100);
        builder.SetMaxSets(100);
        builder.SetPoolFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
        m_pool = builder.Build();
    }
    {
        DescriptorSetLayout::Builder builder{*m_logicalDevice};
        builder.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAll);
        m_setLayout = builder.Build();
    }
}

void UI::Internal_BeginUIFrame(vk::CommandBuffer cmd)
{
    if(!m_hasInitialized) { return; }
    if(m_startedFrame) { return; }
    m_startedFrame = true;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void UI::Internal_EndUIFrame(const vk::CommandBuffer cmd)
{
    if(!m_startedFrame) { return; }

    ImGui::Render();
    ImGui::EndFrame();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    m_startedFrame = false;
}

void UI::Internal_DrawWidgetList()
{
    for(auto& widget: m_widgets) { widget->Draw(); }
}

void UI::Internal_Debug_DrawMetrics(const s16& draws, const glm::vec3& camPosition)
{
    static UiWidget debugWidget{"Debug data", true, {1920 - 400, 1080 - 500}, {400, 500}, 0};

    debugWidget.Add([&]() {
        ImGui::Text("Draws: %i", draws);
        ImGui::Text("Camera Position: %f, %f, %f", camPosition.x, camPosition.y, camPosition.z);
    });

    debugWidget.Draw();
    debugWidget.ClearQueue();
}

void UI::Internal_AddWidgetToList(UiWidget* widg) { m_widgets.push_back(widg); }

void UI::Internal_PopWidgetAtIndex(const n32 index) { m_widgets.erase(m_widgets.begin() + index); }

std::vector<UiWidget*> UI::Internal_GetWidgetList() { return m_widgets; }

}; // namespace Humongous
