#include <thread>
#include <vulkan_app.hpp>

namespace Humongous
{

VulkanApp::VulkanApp() { Init(); }

VulkanApp::~VulkanApp() { Cleanup(); }

void VulkanApp::Init()
{
    window = std::make_unique<Window>();
    instance = std::make_unique<Instance>();
    physicalDevice = std::make_unique<PhysicalDevice>(*instance, *window);
    logicalDevice = std::make_unique<LogicalDevice>(*instance, *physicalDevice);
    swapChain = std::make_unique<SwapChain>(*window, *physicalDevice, *logicalDevice);
}

void VulkanApp::Cleanup() {}

void VulkanApp::Run()
{
    while(!window->ShouldWindowClose())
    {
        glfwPollEvents();

        if(glfwGetKey(window->GetWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) { glfwSetWindowShouldClose(window->GetWindow(), true); }

        if(!window->IsFocused() || window->IsMinimized()) { std::this_thread::sleep_for(std::chrono::milliseconds(300)); }
    }
    vkDeviceWaitIdle(logicalDevice->GetVkDevice());
}

} // namespace Humongous
