#include <logger.hpp>
#include <thread>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <vulkan_app.hpp>

#include <abstractions/buffer.hpp>

namespace Humongous
{

VulkanApp::VulkanApp() { Init(); }

VulkanApp::~VulkanApp() { m_mainDeletionQueue.Flush(); }

void VulkanApp::Init()
{
    m_window = std::make_unique<Window>();
    m_instance = std::make_unique<Instance>();
    m_physicalDevice = std::make_unique<PhysicalDevice>(*m_instance, *m_window);
    m_logicalDevice = std::make_unique<LogicalDevice>(*m_instance, *m_physicalDevice);

    CreatePipelineLayout();

    Buffer buffer{*m_logicalDevice, 256, 1, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

    buffer.Map();
    buffer.UnMap();

    // TODO: move this
    RenderPipeline::PipelineConfigInfo info;
    HGINFO("default configuring pipeline layout...");
    info = RenderPipeline::DefaultPipelineConfigInfo();
    HGINFO("success");
    info.pipelineLayout = pipelineLayout;
    m_renderPipeline = std::make_unique<RenderPipeline>(*m_logicalDevice, info);

    m_mainDeletionQueue.PushDeletor([&]() {
        m_logicalDevice.reset();
        m_physicalDevice.reset();
        m_window.reset();
        m_instance.reset();
    });

    m_mainDeletionQueue.PushDeletor([&]() {
        vkDestroyPipelineLayout(m_logicalDevice->GetVkDevice(), pipelineLayout, nullptr);
        m_renderPipeline.reset();
    });

    m_renderer = std::make_unique<Renderer>(*m_window, *m_logicalDevice, *m_physicalDevice, m_logicalDevice->GetVmaAllocator());

    m_mainDeletionQueue.PushDeletor([&]() { m_renderer.reset(); });
}

void VulkanApp::CreatePipelineLayout()
{
    HGINFO("Creating pipeline layout...");
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if(vkCreatePipelineLayout(m_logicalDevice->GetVkDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create pipeline layout");
    }

    HGINFO("Created pipeline layout");
}

void VulkanApp::Run()
{
    while(!m_window->ShouldWindowClose())
    {
        glfwPollEvents();

        if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwGetKey(m_window->GetWindow(), GLFW_KEY_Q) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(m_window->GetWindow(), true);
        }

        if(!m_window->IsFocused() || m_window->IsMinimized()) { std::this_thread::sleep_for(std::chrono::milliseconds(300)); }
        else
        {
            if(auto cmd = m_renderer->BeginFrame())
            {
                m_renderer->BeginRendering(cmd);

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_renderPipeline->GetPipeline());

                vkCmdDraw(cmd, 3, 1, 0, 0);

                m_renderer->EndRendering(cmd);

                m_renderer->EndFrame();
            }
        }
    }
    vkDeviceWaitIdle(m_logicalDevice->GetVkDevice());
}

} // namespace Humongous
