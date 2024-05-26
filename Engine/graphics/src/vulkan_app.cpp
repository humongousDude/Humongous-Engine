#include "allocator.hpp"
#include "camera.hpp"
#include "model.hpp"
#include <keyboard_handler.hpp>
#include <logger.hpp>
#include <vulkan_app.hpp>
#define VMA_IMPLEMENTATION
#include "asset_manager.hpp"
#include <vk_mem_alloc.h>

namespace Humongous
{
VulkanApp::VulkanApp()
{
    Init();
    LoadGameObjects();
}

VulkanApp::~VulkanApp()
{
    vkDeviceWaitIdle(m_logicalDevice->GetVkDevice());
    m_mainDeletionQueue.Flush();
}

void VulkanApp::Init()
{
    m_window = std::make_unique<Window>();
    m_instance = std::make_unique<Instance>();
    m_physicalDevice = std::make_unique<PhysicalDevice>(*m_instance, *m_window);
    m_logicalDevice = std::make_unique<LogicalDevice>(*m_instance, *m_physicalDevice);

    Systems::AssetManager::Get().Init();

    Allocator::Get().Initialize(m_logicalDevice.get());

    m_renderer = std::make_unique<Renderer>(*m_window, *m_logicalDevice, *m_physicalDevice, m_logicalDevice->GetVmaAllocator(),
                                            VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_D32_SFLOAT);
    m_mainDeletionQueue.PushDeletor([&]() {
        m_simpleRenderSystem.reset();
        m_skyboxRenderSystem.reset();
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
    model = std::make_shared<Model>(m_logicalDevice.get(),
                                    Systems::AssetManager::Get().GetAsset(Systems::AssetManager::AssetType::MODEL, "employee"), 1.00);

    // model = std::make_shared<Model>(m_logicalDevice.get(), "C:/dev/Coding/Github Repos/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf", 1);

    GameObject obj = GameObject::CreateGameObject();
    obj.transform.translation = {0.0f, 0.0f, 0.0f};
    obj.transform.rotation = {glm::radians(00.0f), 0, 0};
    obj.transform.scale = {1.00f, 1.00f, 1.00f};

    obj.model = model;

    m_gameObjects.emplace(obj.GetId(), std::move(obj));

    HGINFO("Loaded game objects");
    m_mainDeletionQueue.PushDeletor([&]() { m_gameObjects.clear(); });
}

void VulkanApp::HandleInput(float frameTime, GameObject& viewerObject)
{
    static double oldX, oldY, newX, newY = 0;

    KeyboardHandler handler;

    if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_I) == GLFW_PRESS) { m_window->HideCursor(); }
    if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_O) == GLFW_PRESS) { m_window->ShowCursor(); }

    glfwGetCursorPos(m_window->GetWindow(), &newX, &newY);
    if(!m_window->IsCursorHidden())
    {
        newX = oldX = m_window->GetExtent().width / 2.f;
        newY = oldY = m_window->GetExtent().height / 2.f;
        return;
    }

    double deltaX = newX - oldX;
    double deltaY = newY - oldY;

    KeyboardHandler::InputData data{frameTime, viewerObject, KeyboardHandler::Movements::NONE, deltaX, deltaY};

    handler.ProcessInput(data);

    if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_W) == GLFW_PRESS)
    {
        data.movementType = KeyboardHandler::Movements::FORWARD;
        handler.ProcessInput(data);
    }
    if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_S) == GLFW_PRESS)
    {
        data.movementType = KeyboardHandler::Movements::BACKWARD;
        handler.ProcessInput(data);
    }
    if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_A) == GLFW_PRESS)
    {
        data.movementType = KeyboardHandler::Movements::LEFT;
        handler.ProcessInput(data);
    }
    if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_D) == GLFW_PRESS)
    {
        data.movementType = KeyboardHandler::Movements::RIGHT;
        handler.ProcessInput(data);
    }
    if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_Q) == GLFW_PRESS)
    {
        data.movementType = KeyboardHandler::Movements::DOWN;
        handler.ProcessInput(data);
    }
    if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_E) == GLFW_PRESS)
    {
        data.movementType = KeyboardHandler::Movements::UP;
        handler.ProcessInput(data);
    }

    oldX = newX;
    oldY = newY;
}

void VulkanApp::Run()
{
    Camera cam{m_logicalDevice.get()};

    float aspect = m_renderer->GetAspectRatio();
    cam.SetViewTarget(glm::vec3(-1.0f, -2.0f, -2.0f), glm::vec3(0.0f, 0.0f, 2.5f));

    std::vector<VkDescriptorSetLayout> simpleLayouts = {cam.GetDescriptorSetLayout(), cam.GetParamDescriptorSetLayout()};
    std::vector<VkDescriptorSetLayout> skyboxLayouts = {cam.GetDescriptorSetLayout()};

    m_simpleRenderSystem = std::make_unique<SimpleRenderSystem>(*m_logicalDevice, simpleLayouts);
    m_skyboxRenderSystem = std::make_unique<SkyboxRenderSystem>(m_logicalDevice.get(), "papermill", skyboxLayouts);

    GameObject viewerObject = GameObject::CreateGameObject();
    viewerObject.transform.translation.z = -2.5f;
    cam.SetViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

    auto currentTime = std::chrono::high_resolution_clock::now();

    HGINFO("Running...");
    while(!m_window->ShouldWindowClose())
    {
        glfwPollEvents();
        auto  newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;

        aspect = m_renderer->GetAspectRatio();

        cam.SetViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

        cam.SetPerspectiveProjection(glm::radians(80.0f), aspect, 0.1f, 1000.0f);

        HandleInput(frameTime, viewerObject);

        if(!m_window->IsMinimized() && m_window->IsFocused())
        {
            if(auto cmd = m_renderer->BeginFrame())
            {

                RenderData data{.commandBuffer = cmd,
                                .uboSets = {cam.GetDescriptorSet(m_renderer->GetFrameIndex())},
                                .sceneSets = {cam.GetParamDescriptorSet(m_renderer->GetFrameIndex())},
                                .gameObjects = m_gameObjects,
                                .frameIndex = m_renderer->GetFrameIndex(),
                                .cam = cam};

                cam.UpdateUBO(m_renderer->GetFrameIndex(), viewerObject.transform.translation);

                m_renderer->BeginRendering(cmd);

                m_skyboxRenderSystem->RenderSkybox(data.frameIndex, data.uboSets, cmd);
                m_simpleRenderSystem->RenderObjects(data);

                m_renderer->EndRendering(cmd);

                m_renderer->EndFrame();
            }
        }

        HGINFO("FrameTime: %f", frameTime);
    }

    HGINFO("Quitting...");
}

} // namespace Humongous
