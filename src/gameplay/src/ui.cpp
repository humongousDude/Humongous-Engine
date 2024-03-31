#include "ui.hpp"
#include "logger.hpp"

// lib
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

// std
#include <algorithm>
#include <iterator>

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

    initInfo.Queue = m_logicalDevice->GetGraphicsQueue();
    // initInfo.QueueFamily = m_logicalDevice->GetGraphicsQueueIndex();
    initInfo.PhysicalDevice = m_logicalDevice->GetPhysicalDevice().GetVkPhysicalDevice();
    initInfo.DescriptorPool = m_pool->GetRawPoolHandle();
    initInfo.UseDynamicRendering = true;
    initInfo.ColorAttachmentFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.Subpass = 0;
    initInfo.Allocator = nullptr;
    ImGui_ImplVulkan_Init(&initInfo, VK_NULL_HANDLE);

    InitPipeline();
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

    VkPushConstantRange pcRange{};
    pcRange.offset = 0;
    pcRange.size = sizeof(UIPushConstantBlock);
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    // layoutCI.setLayoutCount = 1;
    // layoutCI.pSetLayouts = layout;
    // layoutCI.pushConstantRangeCount = 1;
    // layoutCI.pPushConstantRanges = &pcRange;

    if(vkCreatePipelineLayout(m_logicalDevice->GetVkDevice(), &layoutCI, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create UI pipeline layout!");
    }

    RenderPipeline::PipelineConfigInfo pipelineCI = RenderPipeline::DefaultPipelineConfigInfo();
    pipelineCI.colorAttachmentFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    pipelineCI.renderingInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipelineCI.depthStencilInfo.depthTestEnable = VK_FALSE;
    pipelineCI.depthStencilInfo.depthWriteEnable = VK_FALSE;
    pipelineCI.pipelineLayout = m_pipelineLayout;
    pipelineCI.vertShaderPath = "compiledShaders/nothing.vert.glsl.spv";
    pipelineCI.fragShaderPath = "compiledShaders/nothing.frag.glsl.spv";

    std::vector<VkVertexInputBindingDescription> inptBindings(1);
    inptBindings[0].binding = 0;
    inptBindings[0].stride = sizeof(ImDrawVert);
    inptBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::copy(inptBindings.begin(), inptBindings.end(), std::back_inserter(pipelineCI.inputBindings));

    std::vector<VkVertexInputAttributeDescription> attribBindings(3);
    attribBindings[0].binding = 0;
    attribBindings[0].location = 0;
    attribBindings[0].offset = offsetof(ImDrawVert, pos);
    attribBindings[0].format = VK_FORMAT_R32G32_SFLOAT;

    attribBindings[1].binding = 0;
    attribBindings[1].location = 1;
    attribBindings[1].offset = offsetof(ImDrawVert, uv);
    attribBindings[1].format = VK_FORMAT_R32G32_SFLOAT;

    attribBindings[2].binding = 0;
    attribBindings[2].location = 2;
    attribBindings[2].offset = offsetof(ImDrawVert, col);
    attribBindings[2].format = VK_FORMAT_R8G8B8A8_UNORM;

    std::copy(attribBindings.begin(), attribBindings.end(), std::back_inserter(pipelineCI.attribBindings));

    pipelineCI.bindless = false;

    m_renderPipeline = std::make_unique<RenderPipeline>(*m_logicalDevice, pipelineCI);
}

void UI::Draw(VkCommandBuffer cmd)
{
    m_renderPipeline->Bind(cmd);

    // UIPushConstantBlock pushConstBlock;
    // pushConstBlock.scale = glm::vec2(.5f);
    // pushConstBlock.translate = glm::vec2(-1.0f);
    // vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UIPushConstantBlock), &pushConstBlock);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

}; // namespace Humongous
