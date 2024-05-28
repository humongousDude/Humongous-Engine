#include "ui.hpp"
#include "asset_manager.hpp"
#include "logger.hpp"

// lib
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "glm/glm.hpp"

namespace Humongous
{

struct UIPushConstantBlock
{
    glm::vec2 scale;
    glm::vec2 translate;
};

void UI::Init(class Instance* instance, LogicalDevice* logicalDevice, Window* window, Renderer* renderer)
{
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window->GetWindow(), true);
    m_logicalDevice = logicalDevice;

    InitDescriptorThings();

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

    renderingInfo = RenderPipeline::DefaultPipelineConfigInfo().renderingInfo;
    renderingInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

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
    initInfo.PipelineRenderingCreateInfo = renderingInfo;
    ImGui_ImplVulkan_Init(&initInfo);

    InitPipeline();
}

void UI::Shutdown()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyPipelineLayout(m_logicalDevice->GetVkDevice(), m_pipelineLayout, nullptr);
}

void UI::InitDescriptorThings()
{
    {
        DescriptorPool::Builder builder{*m_logicalDevice};
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 1000);
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000);
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000);
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000);
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000);
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000);
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000);
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000);
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000);
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000);
        builder.AddPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000);

        builder.SetMaxSets(1000);
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

    if(vkCreatePipelineLayout(m_logicalDevice->GetVkDevice(), &layoutCI, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create UI pipeline layout!");
    }

    RenderPipeline::PipelineConfigInfo pipelineCI = RenderPipeline::DefaultPipelineConfigInfo();
    pipelineCI.colorAttachmentFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    pipelineCI.renderingInfo = renderingInfo;
    pipelineCI.depthStencilInfo.depthTestEnable = VK_FALSE;
    pipelineCI.depthStencilInfo.depthWriteEnable = VK_FALSE;
    pipelineCI.pipelineLayout = m_pipelineLayout;
    pipelineCI.vertShaderPath = Systems::AssetManager::Get().GetAsset(Systems::AssetManager::AssetType::SHADER, "nothing.vert");
    pipelineCI.fragShaderPath = Systems::AssetManager::Get().GetAsset(Systems::AssetManager::AssetType::SHADER, "nothing.frag");

    m_renderPipeline = std::make_unique<RenderPipeline>(*m_logicalDevice, pipelineCI);
}

void UI::Draw(VkCommandBuffer cmd)
{
    m_renderPipeline->Bind(cmd);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::Render();
    ImGui::EndFrame();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

}; // namespace Humongous
