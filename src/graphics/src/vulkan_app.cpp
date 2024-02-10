#include "camera.hpp"
#include "defines.hpp"
#include "gameobject.hpp"
#include "keyboard_handler.hpp"
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
    // copied from brendan galea's vulkan tutorial series
    std::vector<Vertex> rect_vertices(4);

    rect_vertices[0].position = {0.5, -0.5, 0};
    rect_vertices[1].position = {0.5, 0.5, 0};
    rect_vertices[2].position = {-0.5, -0.5, 0};
    rect_vertices[3].position = {-0.5, 0.5, 0};
    rect_vertices[0].uv = {1, 1};
    rect_vertices[1].uv = {1, 0};
    rect_vertices[2].uv = {0, 1};
    rect_vertices[3].uv = {0, 0};

    std::vector<u32> rect_indices(6);
    rect_indices[0] = 0;
    rect_indices[1] = 1;
    rect_indices[2] = 2;
    rect_indices[3] = 2;
    rect_indices[4] = 1;
    rect_indices[5] = 3;

    std::shared_ptr<Model> model = std::make_shared<Model>(*m_logicalDevice, rect_vertices, rect_indices, "./textures/texture.jpg");

    GameObject obj = GameObject::CreateGameObject();
    obj.transform.translation = {0.0f, 0.0f, -0.5f};
    obj.model = model;

    m_gameObjects.emplace(obj.GetId(), std::move(obj));
    HGINFO("Loaded game objects");

    m_mainDeletionQueue.PushDeletor([&]() { m_gameObjects.clear(); });
}

void VulkanApp::Run()
{
    HGINFO("Running...");

    Camera cam{*m_logicalDevice};

    float aspect = m_renderer->GetAspectRatio();
    cam.SetViewTarget(glm::vec3(-1.0f, -2.0f, -2.0f), glm::vec3(0.0f, 0.0f, 2.5f));
    m_simpleRenderSystem = std::make_unique<SimpleRenderSystem>(*m_logicalDevice, cam.GetDescriptorSetLayout());

    GameObject viewerObject = GameObject::CreateGameObject();
    viewerObject.transform.translation.z = -2.5f;
    cam.SetViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

    KeyboardHandler handler{};

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
                RenderData data{cmd, cam.GetDescriptorSet(m_renderer->GetFrameIndex()), m_gameObjects, m_renderer->GetFrameIndex()};

                cam.UpdateUBO(m_renderer->GetFrameIndex());

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
