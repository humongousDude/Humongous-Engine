#include "vulkan_app.hpp"
#include "allocator.hpp"
#include "camera.hpp"
#include "globals.hpp"
#include "keyboard_handler.hpp"
#include "logger.hpp"
#include "model.hpp"
#include "ui/ui.hpp"
#define VMA_IMPLEMENTATION
#include "asset_manager.hpp"
#include "vk_mem_alloc.h"

#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "ui/widget.hpp"

namespace Humongous
{
VulkanApp::VulkanApp(int argc, char* argv[])
{
    Init(argc, argv);
    LoadGameObjects();
}

VulkanApp::~VulkanApp()
{
    m_logicalDevice->GetVkDevice().waitIdle();
    m_mainDeletionQueue.Flush();
}

void VulkanApp::Init(int argc, char* argv[])
{
    m_window = std::make_unique<Window>();
    m_instance = std::make_unique<Instance>();
    m_physicalDevice = std::make_unique<PhysicalDevice>(*m_instance, *m_window);
    m_logicalDevice = std::make_unique<LogicalDevice>(*m_instance, *m_physicalDevice);

    if(argc > 1)
    {
        std::vector<std::string> paths;
        for(int i = 1; i < argc; ++i) { paths.push_back(argv[i]); }
        Systems::AssetManager::Init(&paths);
    }
    else
    {
        HGINFO("Launch the engine with absolute paths to extra directories for the asset manager to look for models in");
        Systems::AssetManager::Init();
    }

    Allocator::Initialize(m_logicalDevice.get());

    UI::Init(m_instance.get(), m_logicalDevice.get(), m_window.get());

    m_renderer = std::make_unique<Renderer>(*m_window, *m_logicalDevice, *m_physicalDevice, m_logicalDevice->GetVmaAllocator(),
                                            VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_D32_SFLOAT);

    m_cam = std::make_unique<Camera>(m_logicalDevice.get());

    m_mainDeletionQueue.PushDeletor([&]() {
        m_simpleRenderSystem.reset();
        m_skyboxRenderSystem.reset();
        m_renderer.reset();
        m_cam.reset();
        UI::Shutdown();
        Allocator::Shutdown();
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
    model =
        std::make_shared<Model>(m_logicalDevice.get(), Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::MODEL, "Sponza"), 1.00);

    GameObject obj = GameObject::CreateGameObject();
    obj.transform.translation = {0.0f, 0.0f, 0.0f};
    obj.transform.rotation = {glm::radians(-90.0f), 0, 0};
    obj.transform.scale = {0.50f, 0.50f, 0.50f};
    obj.SetModel(model);

    m_gameObjects.emplace(obj.GetId(), std::move(obj));

    std::shared_ptr<Model> model2;
    model2 = std::make_shared<Model>(m_logicalDevice.get(),
                                     Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::MODEL, "DamagedHelmet"), 1.00);

    GameObject obj2 = GameObject::CreateGameObject();
    obj2.transform.translation = {1.0f, 0.0f, 0.0f};
    obj2.transform.rotation = {glm::radians(180.0f), 0, 0};
    obj2.transform.scale = {0.50f, 0.50f, 0.50f};
    obj2.SetModel(model2);

    m_gameObjects.emplace(obj2.GetId(), std::move(obj2));

    std::shared_ptr<Model> model3;
    model3 = std::make_shared<Model>(m_logicalDevice.get(), Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::MODEL, "old_hunter"),
                                     1.00);
    GameObject obj3 = GameObject::CreateGameObject();
    obj3.transform.translation = {-1.0f, 0.0f, 0.0f};
    obj3.transform.rotation = {glm::radians(180.0f), 0, 0};
    obj3.transform.scale = {0.50f, 0.50f, 0.50f};
    obj3.SetModel(model3);

    m_gameObjects.emplace(obj3.GetId(), std::move(obj3));

    HGINFO("Loaded game objects");
    m_mainDeletionQueue.PushDeletor([&]() { m_gameObjects.clear(); });
}

void VulkanApp::HandleInput(float frameTime, GameObject& viewerObject)
{
    static double oldX, oldY, newX, newY = 0;

    KeyboardHandler handler;

    if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_I) == GLFW_PRESS && !m_window->IsCursorHidden()) { m_window->HideCursor(); }
    if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_O) == GLFW_PRESS ||
       glfwGetKey(m_window->GetWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS && m_window->IsCursorHidden())
    {
        m_window->ShowCursor();
    }

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
    float aspect = m_renderer->GetAspectRatio();
    m_cam->SetViewTarget(glm::vec3(-1.0f, -2.0f, -2.0f), glm::vec3(0.0f, 0.0f, 2.5f));

    std::vector<VkDescriptorSetLayout> simpleLayouts = {m_cam->GetDescriptorSetLayout(), m_cam->GetParamDescriptorSetLayout()};
    std::vector<VkDescriptorSetLayout> skyboxLayouts = {m_cam->GetDescriptorSetLayout()};

    m_simpleRenderSystem = std::make_unique<SimpleRenderSystem>(*m_logicalDevice, simpleLayouts);
    m_skyboxRenderSystem = std::make_unique<SkyboxRenderSystem>(m_logicalDevice.get(), "papermill", skyboxLayouts);

    GameObject viewerObject = GameObject::CreateGameObject();
    viewerObject.transform.translation.z = -2.5f;
    m_cam->SetViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

    auto currentTime = std::chrono::high_resolution_clock::now();

    HGINFO("Running...");
    while(!m_window->ShouldWindowClose())
    {
        glfwPollEvents();
        auto  newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;
        HandleInput(frameTime, viewerObject);

        aspect = m_renderer->GetAspectRatio();

        m_cam->SetViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

        m_cam->SetPerspectiveProjection(glm::radians(80.0f), aspect, 0.1f, 1000.0f);

        if(!m_window->IsMinimized() && m_window->IsFocused())
        {
            if(auto cmd = m_renderer->BeginFrame())
            {

                RenderData data{.commandBuffer = cmd,
                                .uboSets = {m_cam->GetDescriptorSet(m_renderer->GetFrameIndex())},
                                .sceneSets = {m_cam->GetParamDescriptorSet(m_renderer->GetFrameIndex())},
                                .gameObjects = m_gameObjects,
                                .frameIndex = m_renderer->GetFrameIndex(),
                                .cam = *m_cam,
                                .camPos = viewerObject.transform.translation};

                m_cam->UpdateUBO(m_renderer->GetFrameIndex(), viewerObject.transform.translation);

                m_renderer->BeginRendering(cmd);

                m_skyboxRenderSystem->RenderSkybox(data.frameIndex, data.uboSets, cmd);
                m_simpleRenderSystem->RenderObjects(data);

                UI::BeginUIFrame(cmd);

                UI::Debug_DrawMetrics(m_simpleRenderSystem->GetObjectsDrawn());

                UI::EndUIFRame(cmd);

                m_renderer->EndRendering(cmd);
                m_renderer->EndFrame();
            }
        }
        Globals::Time::Update(frameTime);
    }

    HGINFO("Quitting...");
}

} // namespace Humongous
