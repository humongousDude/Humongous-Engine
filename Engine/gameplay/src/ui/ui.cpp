#include "ui/ui.hpp"
#include "asset_manager.hpp"
#include "globals.hpp"
#include "logger.hpp"

// lib
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "cmath"
#include "ui/widget.hpp"
#include <vulkan/vk_enum_string_helper.h>

namespace Humongous
{

void UI::Init(class Instance* instance, LogicalDevice* logicalDevice, Window* window)
{
    if(m_hasInited) { return; }

    m_logicalDevice = logicalDevice;

    InitDescriptorThings();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;

    ImGui_ImplGlfw_InitForVulkan(window->GetWindow(), true);

    ImGui::StyleColorsDark();

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance->GetVkInstance();
    initInfo.Device = m_logicalDevice->GetVkDevice();

    // this is silly
    initInfo.MinImageCount = m_logicalDevice->GetPhysicalDevice()
                                 .QuerySwapChainSupport(m_logicalDevice->GetPhysicalDevice().GetVkPhysicalDevice())
                                 .capabilities.surfaceCapabilities.minImageCount +
                             1;
    initInfo.ImageCount = m_logicalDevice->GetPhysicalDevice()
                              .QuerySwapChainSupport(m_logicalDevice->GetPhysicalDevice().GetVkPhysicalDevice())
                              .capabilities.surfaceCapabilities.maxImageCount;

    m_renderingInfo = RenderPipeline::DefaultPipelineConfigInfo().renderingInfo;
    m_renderingInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

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

    InitPipeline();

    m_hasInited = true;
}

void UI::Shutdown()
{
    if(!m_hasInited) { return; }
    HGINFO("Shutting UI down");
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_logicalDevice->GetVkDevice().destroyPipelineLayout(m_pipelineLayout);
    m_renderPipeline.reset();
    m_pool.reset();
    m_setLayout.reset();
    HGINFO("Successfuly shut UI down");
}

void UI::InitDescriptorThings()
{
    {
        DescriptorPool::Builder builder{*m_logicalDevice};
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100);
        builder.SetMaxSets(100);
        builder.SetPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
        m_pool = builder.Build();
    }
    {
        DescriptorSetLayout::Builder builder{*m_logicalDevice};
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL);
        m_setLayout = builder.build();
    }
}

void UI::InitPipeline()
{
    VkDescriptorSetLayout layout[] = {m_setLayout->GetDescriptorSetLayout()};

    vk::PipelineLayoutCreateInfo layoutCI{};
    m_pipelineLayout = m_logicalDevice->GetVkDevice().createPipelineLayout(layoutCI, nullptr);

    RenderPipeline::PipelineConfigInfo pipelineCI = RenderPipeline::DefaultPipelineConfigInfo();
    pipelineCI.colorAttachmentFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    pipelineCI.renderingInfo = m_renderingInfo;
    pipelineCI.depthStencilInfo.depthTestEnable = VK_FALSE;
    pipelineCI.depthStencilInfo.depthWriteEnable = VK_FALSE;
    pipelineCI.pipelineLayout = m_pipelineLayout;
    pipelineCI.vertShaderPath = Systems::AssetManager::Get().GetAsset(Systems::AssetManager::AssetType::SHADER, "nothing.vert");
    pipelineCI.fragShaderPath = Systems::AssetManager::Get().GetAsset(Systems::AssetManager::AssetType::SHADER, "nothing.frag");

    m_renderPipeline = std::make_unique<RenderPipeline>(*m_logicalDevice, pipelineCI);
}

void UI::Draw(VkCommandBuffer cmd)
{
    if(!m_hasInited) { return; }
    m_renderPipeline->Bind(cmd);
    bool show = false;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const ImGuiIO& io = ImGui::GetIO();

    static bool txt = true;
    UiWidget    widg{"Metrics", true, {00, 0}, {225, 075}, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize};
    widg.AddBullet("FPS: %i", static_cast<int>(std::round((1 / Globals::Time::AverageDeltaTime()))));
    widg.AddBullet("FrameTime(ms): %f", Globals::Time::AverageDeltaTime() * 1000);

    widg.Draw();

    ImGui::Render();
    ImGui::EndFrame();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

}; // namespace Humongous
