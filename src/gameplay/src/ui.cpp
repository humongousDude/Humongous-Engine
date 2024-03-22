#include "ui.hpp"
#include "logger.hpp"

// lib
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

namespace Humongous
{
void UI::Init(class Instance* instance, LogicalDevice* logicalDevice, Window* window, Renderer* renderer)
{
    m_logicalDevice = logicalDevice;

    InitDescriptorThings();
    InitPipeline();

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window->GetWindow(), true);
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance->GetVkInstance();
    initInfo.Device = m_logicalDevice->GetVkDevice();
    initInfo.ImageCount = 2;
    initInfo.MinImageCount = 2;
    initInfo.Queue = m_logicalDevice->GetGraphicsQueue();
    initInfo.QueueFamily = m_logicalDevice->GetGraphicsQueueIndex();
    initInfo.PhysicalDevice = m_logicalDevice->GetPhysicalDevice().GetVkPhysicalDevice();
    initInfo.DescriptorPool = m_pool->GetRawPoolHandle();
    initInfo.UseDynamicRendering = true;
    initInfo.ColorAttachmentFormat = renderer->GetSwapChain()->GetSurfaceFormat();
    ImGui_ImplVulkan_Init(&initInfo, VK_NULL_HANDLE);
}

void UI::Shutdown()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_pool.reset();
    m_setLayout.reset();

    vkDestroyPipelineLayout(m_logicalDevice->GetVkDevice(), m_pipelineLayout, nullptr);
    m_renderPipeline.reset();
}

void UI::InitDescriptorThings()
{
    {
        DescriptorPool::Builder builder{*m_logicalDevice};
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
        builder.SetMaxSets(1);
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

    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.setLayoutCount = 1;
    layoutCI.pSetLayouts = layout;

    if(vkCreatePipelineLayout(m_logicalDevice->GetVkDevice(), &layoutCI, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create UI pipeline layout!");
    }

    RenderPipeline::PipelineConfigInfo pipelineCI = RenderPipeline::DefaultPipelineConfigInfo();
    pipelineCI.colorAttachmentFormat = VK_FORMAT_B8G8R8A8_UNORM;
    pipelineCI.renderingInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipelineCI.pipelineLayout = m_pipelineLayout;
    pipelineCI.vertShaderPath = "compiledShaders/nothing.vert.glsl.spv";
    pipelineCI.fragShaderPath = "compiledShaders/nothing.frag.glsl.spv";

    m_renderPipeline = std::make_unique<RenderPipeline>(*m_logicalDevice, pipelineCI);
}

void UI::Draw(VkCommandBuffer cmd)
{
    m_renderPipeline->Bind(cmd);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

}; // namespace Humongous
