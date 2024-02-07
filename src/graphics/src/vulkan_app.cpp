#include "gameobject.hpp"
#include <logger.hpp>
#include <thread>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <vulkan_app.hpp>

#include <abstractions/buffer.hpp>
#include <abstractions/descriptor_layout.hpp>
#include <abstractions/descriptor_pool.hpp>
#include <abstractions/descriptor_writer.hpp>

namespace Humongous
{

VulkanApp::VulkanApp()
{
    Init();
    LoadGameObjects();
}

VulkanApp::~VulkanApp() { m_mainDeletionQueue.Flush(); }

void VulkanApp::Init()
{
    m_window = std::make_unique<Window>();
    m_instance = std::make_unique<Instance>();
    m_physicalDevice = std::make_unique<PhysicalDevice>(*m_instance, *m_window);
    m_logicalDevice = std::make_unique<LogicalDevice>(*m_instance, *m_physicalDevice);
    m_renderer = std::make_unique<Renderer>(*m_window, *m_logicalDevice, *m_physicalDevice, m_logicalDevice->GetVmaAllocator());
    m_simpleRenderSystem = std::make_unique<SimpleRenderSystem>(*m_logicalDevice);

    m_mainDeletionQueue.PushDeletor([&]() {
        m_simpleRenderSystem.reset();
        m_renderer.reset();
        m_logicalDevice.reset();
        m_physicalDevice.reset();
        m_window.reset();
        m_instance.reset();
    });
}

void VulkanApp::LoadGameObjects()
{
    HGINFO("Loading game objects...");
    std::vector<Vertex> vertices{{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}}, {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}, {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

    std::shared_ptr<Model> model = std::make_shared<Model>(*m_logicalDevice, vertices);

    GameObject obj = GameObject::CreateGameObject();
    obj.model = model;

    m_gameObjects.emplace(obj.GetId(), std::move(obj));
    HGINFO("Loaded game objects...");

    m_mainDeletionQueue.PushDeletor([&]() { m_gameObjects.clear(); });
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

                m_simpleRenderSystem->RenderObjects(m_gameObjects, cmd);

                m_renderer->EndRendering(cmd);

                m_renderer->EndFrame();
            }
        }
    }
    vkDeviceWaitIdle(m_logicalDevice->GetVkDevice());
}

} // namespace Humongous
