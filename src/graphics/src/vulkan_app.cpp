#include <logger.hpp>
#include <thread>
#include <vulkan_app.hpp>

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
    m_swapChain = std::make_unique<SwapChain>(*m_window, *m_physicalDevice, *m_logicalDevice);

    CreatePipelineLayout();

    RenderPipeline::PipelineConfigInfo info;
    HGINFO("default configuring pipeline layout...");
    info = RenderPipeline::DefaultPipelineConfigInfo();
    HGINFO("success");
    info.pipelineLayout = pipelineLayout;
    m_renderPipeline = std::make_unique<RenderPipeline>(*m_logicalDevice, info);

    m_mainDeletionQueue.PushDeletor([&]() {
        vkDestroyPipelineLayout(m_logicalDevice->GetVkDevice(), pipelineLayout, nullptr);

        m_renderPipeline.reset();

        m_swapChain.reset();
        m_logicalDevice.reset();
        m_physicalDevice.reset();
        m_window.reset();
        m_instance.reset();
    });
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
    }
    vkDeviceWaitIdle(m_logicalDevice->GetVkDevice());
}

} // namespace Humongous
