#include <vulkan_app.hpp>

#include "camera.hpp"
#include <keyboard_handler.hpp>

#include "allocator.hpp"

#include <logger.hpp>

#include <thread>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

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

    m_mainDeletionQueue.PushDeletor([&]() {
        m_simpleRenderSystem.reset();
        m_renderer.reset();
        Allocator::Get().Shutdown();
        m_logicalDevice.reset();
        m_physicalDevice.reset();
        m_window.reset();
        m_instance.reset();
    });
}

void VulkanApp::LoadGameObjects()
{

    HGINFO("Loading game objects...");

    std::shared_ptr<Model> model;
    model = std::make_shared<Model>(m_logicalDevice.get(), "models/old_hunter.glb", 1);

    GameObject obj = GameObject::CreateGameObject();
    obj.transform.translation = {0.0f, 0.0f, -1.0f};
    obj.transform.rotation = {glm::radians(90.0f), 0.0f, 0.0f};

    obj.model = model;

    m_gameObjects.emplace(obj.GetId(), std::move(obj));

    HGINFO("Loaded game objects");
    m_mainDeletionQueue.PushDeletor([&]() { m_gameObjects.clear(); });
}

void VulkanApp::Run()
{
    Camera cam{m_logicalDevice.get()};

    float aspect = m_renderer->GetAspectRatio();
    cam.SetViewTarget(glm::vec3(-1.0f, -2.0f, -2.0f), glm::vec3(0.0f, 0.0f, 2.5f));

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {cam.GetDescriptorSetLayout(), cam.GetParamDescriptorSetLayout()};

    m_simpleRenderSystem = std::make_unique<SimpleRenderSystem>(*m_logicalDevice, descriptorSetLayouts);

    GameObject viewerObject = GameObject::CreateGameObject();
    viewerObject.transform.translation.z = -2.5f;
    cam.SetViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

    KeyboardHandler handler{};

    HGINFO("Running...");
    while(!m_window->ShouldWindowClose())
    {
        glfwPollEvents();

        aspect = m_renderer->GetAspectRatio();

        handler.MoveInPlaneXZ(m_window->GetWindow(), 0.01f, viewerObject);
        cam.SetViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

        cam.SetPerspectiveProjection(glm::radians(90.0f), aspect, 0.1f, 1000.0f);

        if(!m_window->IsFocused() || m_window->IsMinimized()) { std::this_thread::sleep_for(std::chrono::milliseconds(300)); }
        else
        {
            if(auto cmd = m_renderer->BeginFrame())
            {
                RenderData data{cmd,
                                {cam.GetDescriptorSet(m_renderer->GetFrameIndex()), cam.GetParamDescriptorSet(m_renderer->GetFrameIndex())},
                                {cam.GetDescriptorSet(m_renderer->GetFrameIndex()), cam.GetParamDescriptorSet(m_renderer->GetFrameIndex())},
                                m_gameObjects,
                                m_renderer->GetFrameIndex()};

                cam.UpdateUBO(m_renderer->GetFrameIndex(), viewerObject.transform.translation);

                m_renderer->BeginRendering(cmd);

                m_simpleRenderSystem->RenderObjects(data);

                m_renderer->EndRendering(cmd);

                m_renderer->EndFrame();
            }
        }
    }
    vkDeviceWaitIdle(m_logicalDevice->GetVkDevice());

    HGINFO("Quitting...");
}

} // namespace Humongous
