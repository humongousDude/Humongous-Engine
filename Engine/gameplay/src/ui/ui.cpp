#include "ui/ui.hpp"
#include "abstractions/image.hpp"
#include "instance.hpp"
#include "logger.hpp"
#include "logical_device.hpp"
#include "render_pipeline.hpp"

// lib
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "imgui_internal.h"

#include "ui/widget.hpp"
#include <vulkan/vk_enum_string_helper.h>

namespace Humongous
{

void UI::Internal_Init(const class IInstance& instance, const ILogicalDevice& logicalDevice, const Window& window)
{
    if(m_hasInitialized) { return; }
    HGINFO("Initializing UI...");

    m_logicalDevice = &logicalDevice;

    InitDescriptorThings();

    IMGUI_CHECKVERSION();
    ImGuiContext* io = ImGui::CreateContext();
    io->IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Causing too many issues.
    // io->IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui_ImplSDL3_InitForVulkan(window.GetWindow());

    ImGui::StyleColorsDark();

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance.GetVkInstance();
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
    vk::Format colorFormat = vk::Format::eR8G8B8A8Unorm;
    m_renderingInfo.pColorAttachmentFormats = &colorFormat;

    ImGui_ImplVulkan_PipelineInfo imguiInfo{};
    imguiInfo.MSAASamples = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1);
    imguiInfo.PipelineRenderingCreateInfo = m_renderingInfo;
    imguiInfo.Subpass = 0;
    imguiInfo.SwapChainImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    initInfo.Queue = m_logicalDevice->GetGraphicsQueue();
    initInfo.QueueFamily = m_logicalDevice->GetGraphicsQueueIndex();
    initInfo.PhysicalDevice = m_logicalDevice->GetPhysicalDevice().GetVkPhysicalDevice();
    initInfo.DescriptorPool = m_pool->GetRawPoolHandle();
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.Allocator = nullptr;
    initInfo.PipelineInfoMain = imguiInfo;
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

void UI::Internal_BeginUIFrame()
{
    if(!m_hasInitialized) { return; }
    if(m_startedFrame) { return; }
    m_startedFrame = true;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(1, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
}

// Called with the renderer's scene image
void UI::Internal_RecreateViewportResources(const class Image& sceneImage, const u32& index)
{
    if(m_sceneTextureID[index] != VK_NULL_HANDLE) { ImGui_ImplVulkan_RemoveTexture(m_sceneTextureID[index]); }

    const auto descInfo = sceneImage.GetDescriptorInfo();
    m_sceneTextureID[index] =
        ImGui_ImplVulkan_AddTexture(descInfo.sampler, descInfo.imageView, static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));
    m_sceneTextureRef[index] = ImTextureRef{m_sceneTextureID[index]};
}

void UI::Internal_RenderViewport()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("Viewport", nullptr);
    ImGui::SetWindowSize(ImVec2(static_cast<f32>(800), static_cast<f32>(600)), ImGuiCond_Once);

    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    m_viewportWidth = static_cast<u32>(viewportPanelSize.x);
    m_viewportHeight = static_cast<u32>(viewportPanelSize.y);
    HGINFO("Viewport size: %ix%i", m_viewportWidth, m_viewportHeight);

    ImGui::Image(m_sceneTextureRef[m_currentFrameIndex], viewportPanelSize, ImVec2(0, 0), ImVec2(1, 1));

    ImGui::End();
    ImGui::PopStyleVar();
}

void UI::Internal_EndUIFrame(const vk::CommandBuffer cmd)
{
    if(!m_startedFrame) { return; }

    ImGui::Render();
    ImGui::EndFrame();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    m_startedFrame = false;

    m_currentFrameIndex = (m_currentFrameIndex + 1) % static_cast<u32>(Globals::Limits::MaxFramesInFlight);
}

void UI::Internal_DrawWidgetList()
{
    for(auto& widget: m_widgets) { widget->Draw(); }
}

void UI::Internal_Debug_DrawMetrics(const s16& draws, const Eigen::Vector3f& camPosition)
{
    static UiWidget debugWidget{"Debug data", true, {1920 - 400, 1080 - 500}, {400, 500}, 0};

    debugWidget.Add([&]() {
        ImGui::Text("Draws: %i", draws);
        ImGui::Text("Camera Position: %f, %f, %f", camPosition.x(), camPosition.y(), camPosition.z());
        ImGui::Text("Frametime: %.2f ms", Globals::Time::AverageDeltaTime() * 1000.0f);
    });

    debugWidget.Draw();
    debugWidget.ClearQueue();
}

void UI::Internal_AddWidgetToList(UiWidget* widg) { m_widgets.push_back(widg); }

void UI::Internal_PopWidgetAtIndex(const u32 index) { m_widgets.erase(m_widgets.begin() + index); }

std::vector<UiWidget*> UI::Internal_GetWidgetList() { return m_widgets; }

}; // namespace Humongous
