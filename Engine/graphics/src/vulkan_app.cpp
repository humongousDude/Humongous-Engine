#include "vulkan_app.hpp"
#include "allocator.hpp"
#include "camera.hpp"
#include "extra.hpp"
#include "globals.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "keyboard_handler.hpp"
#include "logger.hpp"
#include "resource_manager.hpp"
#define VMA_IMPLEMENTATION
#include "asset_manager.hpp"
#include "vk_mem_alloc.h"

#include "ui/ui.hpp"

namespace Humongous
{
VulkanApp::VulkanApp(const int argc, char* argv[])
{
    Init(argc, argv);
    LoadGameObjects();
}

VulkanApp::~VulkanApp()
{
    m_logicalDevice->GetVkDevice().waitIdle();
    m_mainDeletionQueue.Flush();
}

void VulkanApp::Init(const int argc, char* argv[])
{
    m_window = std::make_unique<Window>();
    m_instance = std::make_unique<Instance>();
    m_physicalDevice = std::make_unique<PhysicalDevice>(*m_instance, *m_window);
    m_logicalDevice = std::make_unique<LogicalDevice>(*m_instance, *m_physicalDevice);

    if(argc > 1)
    {
        std::vector<std::string> paths;
        for(int i = 1; i < argc; ++i) { paths.emplace_back(argv[i]); }
        Systems::AssetManager::Init(&paths);
    }
    else
    {
        HGINFO("Launch the engine with absolute paths to extra directories for the asset manager to look for models in");
        Systems::AssetManager::Init();
    }

    Allocator::Initialize(m_logicalDevice.get());

    ResourceManager::Init(m_logicalDevice.get());

    UI::Init(m_instance.get(), m_logicalDevice.get(), m_window.get());

    m_renderer = std::make_unique<Renderer>(*m_window, *m_logicalDevice, *m_physicalDevice, m_logicalDevice->GetVmaAllocator(),
                                            VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_D32_SFLOAT);

    m_cam = std::make_unique<Camera>(m_logicalDevice.get());

    std::vector<VkDescriptorSetLayout> simpleLayouts = {m_cam->GetDescriptorSetLayout(), m_cam->GetParamDescriptorSetLayout()};
    std::vector<VkDescriptorSetLayout> skyboxLayouts = {m_cam->GetDescriptorSetLayout()};

    ShaderSet set = {Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "simple.vert"),
                     Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "unlit.frag")};

    m_simpleRenderSystem = std::make_unique<SimpleRenderSystem>(*m_logicalDevice, simpleLayouts, set);
    m_skyboxRenderSystem = std::make_unique<SkyboxRenderSystem>(m_logicalDevice.get(), "papermill", skyboxLayouts);

    m_mainDeletionQueue.PushDeletor([&]() {
        m_simpleRenderSystem.reset();
        m_skyboxRenderSystem.reset();
        m_renderer.reset();
        m_cam.reset();
        UI::Shutdown();
        ResourceManager::Shutdown();
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

    std::shared_ptr<Model> wallModel = ResourceManager::LoadModel("real_wall");

    GameObject wall = GameObject::CreateGameObject();
    wall.transform.translation = glm::vec3(0.0f, 0.0f, 50.f);
    wall.transform.scale = glm::vec3(50, 40, 1);
    wall.transform.rotation = glm::vec3(glm::radians(90.0f), 0.0f, 0.0f);
    wall.SetModel(wallModel);
    wall.name = "Wall";
    m_gameObjects.emplace(wall.GetId(), std::move(wall));

    std::shared_ptr<Model> employeeModel = ResourceManager::LoadModel("employee");

    GameObject employee = GameObject::CreateGameObject();
    employee.transform.translation = glm::vec3(0, 0, 5);
    employee.transform.scale = glm::vec3(1.0f);
    employee.transform.rotation = glm::vec3(glm::radians(90.f), 0.0f, 0.0f);
    employee.name = "Employee";
    employee.SetModel(employeeModel);

    m_gameObjects.emplace(employee.GetId(), std::move(employee));

    std::shared_ptr<Model> carModel = ResourceManager::LoadModel("silly thing");

    GameObject car = GameObject::CreateGameObject();
    car.transform.translation = glm::vec3(0, 0, 150);
    car.transform.scale = glm::vec3(1);
    car.name = "Car";
    car.SetModel(carModel);

    m_gameObjects.emplace(car.GetId(), std::move(car));

    m_mainDeletionQueue.PushDeletor([&]() { m_gameObjects.clear(); });

    HGINFO("Loaded game objects");
}

void VulkanApp::HandleInput(const float frameTime, SDL_Event* event) const
{
    float                      deltaX = 0.0f, deltaY = 0.0f;
    auto  movementType = KeyboardHandler::Movements::NONE;

    KeyboardHandler handler;

    // Handle cursor visibility (This part is fine as is)
    const bool* keyboardState = SDL_GetKeyboardState(nullptr);
    if(keyboardState[SDL_SCANCODE_I] && !m_window->IsCursorHidden()) { m_window->HideCursor(); }
    if((keyboardState[SDL_SCANCODE_O] || keyboardState[SDL_SCANCODE_ESCAPE]) && m_window->IsCursorHidden()) { m_window->ShowCursor(); }
    if(!m_window->IsCursorHidden()) { return; }

    SDL_GetRelativeMouseState(&deltaX, &deltaY);

    if(keyboardState[SDL_SCANCODE_W]) { movementType = KeyboardHandler::Movements::FORWARD; }
    if(keyboardState[SDL_SCANCODE_S])
    {
        if(movementType == KeyboardHandler::Movements::FORWARD)
        {
            movementType = KeyboardHandler::Movements::NONE;
        }
        else { movementType = KeyboardHandler::Movements::BACKWARD; }
    }
    if(keyboardState[SDL_SCANCODE_A])
    {
        if(movementType == KeyboardHandler::Movements::RIGHT)
        {
            movementType = KeyboardHandler::Movements::NONE;
        }
        else if(movementType == KeyboardHandler::Movements::FORWARD)
        {
            movementType = KeyboardHandler::Movements::FORWARD_LEFT;
        }
        else if(movementType == KeyboardHandler::Movements::BACKWARD)
        {
            movementType = KeyboardHandler::Movements::BACKWARD_LEFT;
        }
        else { movementType = KeyboardHandler::Movements::LEFT; }
    }
    if(keyboardState[SDL_SCANCODE_D])
    {
        if(movementType == KeyboardHandler::Movements::LEFT)
        {
            movementType = KeyboardHandler::Movements::NONE;
        }
        else if(movementType == KeyboardHandler::Movements::FORWARD)
        {
            movementType = KeyboardHandler::Movements::FORWARD_RIGHT;
        }
        else if(movementType == KeyboardHandler::Movements::BACKWARD)
        {
            movementType = KeyboardHandler::Movements::BACKWARD_RIGHT;
        }
        else { movementType = KeyboardHandler::Movements::RIGHT; }
    }
    if(keyboardState[SDL_SCANCODE_Q])
    {
        if(movementType == KeyboardHandler::Movements::UP)
        {
            movementType = KeyboardHandler::Movements::NONE;
        }
        else { movementType = KeyboardHandler::Movements::DOWN; }
    }
    if(keyboardState[SDL_SCANCODE_E])
    {
        if(movementType == KeyboardHandler::Movements::DOWN)
        {
            movementType = KeyboardHandler::Movements::NONE;
        }
        else { movementType = KeyboardHandler::Movements::UP; }
    }

    const KeyboardHandler::InputData data{frameTime, movementType, deltaX, deltaY, *m_cam};

    handler.ProcessInput(data);
}

void VulkanApp::Run()
{
    m_cam->UpdateViewMatrix();

    auto currentTime = std::chrono::high_resolution_clock::now();

    UiWidget objectDataWidget{"Object Data", true, {0, 100}, {400, 500}, 0};
    for(auto& [id, obj]: m_gameObjects)
    {
        objectDataWidget.Add([&]() {
            if(ImGui::CollapsingHeader(obj.name.c_str()))
            {
                ImGui::PushID(id);
                ImGui::Text("ID: %i", id);
                ImGui::Text("Position: %f, %f, %f", obj.transform.translation.x, obj.transform.translation.y, obj.transform.translation.z);

                if(ImGui::TreeNode("Bounding box Corners"))
                {
                    for(n32 i = 0; i < obj.GetBoundingBox().corners.size(); ++i)
                    {
                        auto box = obj.GetBoundingBox();
                        ImGui::Text("Corner %i: %f, %f, %f", i, box.corners[i].x, box.corners[i].y, box.corners[i].z);
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
        });
    }

    // objectDataWidget.Add([&]() { ImGui::ShowDemoWindow(); });

    HGINFO("Running...");
    bool      quit = false;
    bool      focused = false;
    bool      minimized = false;
    SDL_Event e;
    while(!quit)
    {
        auto       newTime = std::chrono::high_resolution_clock::now();
        const auto frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;
        Globals::Time::Update(frameTime);

        while(SDL_PollEvent(&e))
        {
            if(e.type == SDL_EVENT_QUIT) { quit = true; }
            ImGui_ImplSDL3_ProcessEvent(&e);

            switch(e.type)
            {
                case SDL_EVENT_WINDOW_MINIMIZED:
                    minimized = true;
                    HGINFO("Window is minimized");
                    break;
                case SDL_EVENT_WINDOW_RESTORED:
                    minimized = false;
                    HGINFO("Window is restored");
                    break;
                case SDL_EVENT_WINDOW_FOCUS_LOST:
                    focused = false;
                    HGINFO("Window lost focus");
                    break;
                case SDL_EVENT_WINDOW_FOCUS_GAINED:
                    focused = true;
                    HGINFO("Window gained focus");
                    break;
                default:;
            }
        }
        HandleInput(frameTime, &e);

        const float aspect = m_renderer->GetAspectRatio();

        m_cam->SetPerspectiveProjection(glm::radians(80.0f), aspect, 0.1f, 1000.0f);

        if(!minimized && focused)
        {
            for(auto& [k, v]: m_gameObjects) { v.Update(); }

            if(const auto cmd = m_renderer->BeginFrame())
            {
                m_cam->Update();
                RenderData data{.commandBuffer = cmd,
                                .uboSets = {m_cam->GetDescriptorSet(m_renderer->GetFrameIndex())},
                                .sceneSets = {m_cam->GetParamDescriptorSet(m_renderer->GetFrameIndex())},
                                .gameObjects = Utils::SortAndCullGameObjects(*m_cam, m_gameObjects),
                                .frameIndex = m_renderer->GetFrameIndex(),
                                .cam = *m_cam,
                                .renderer = *m_renderer,
                                .camPos = m_cam->GetPosition()};

                m_renderer->BeginDepthPrePass(cmd);

                m_simpleRenderSystem->DepthOnlyRender(data);

                m_renderer->EndDepthPrePass(cmd);

                m_renderer->DoGPUOcclusionCulling(cmd, data, *m_cam);

                m_renderer->BeginRendering(cmd);

                m_skyboxRenderSystem->RenderSkybox(data.frameIndex, data.uboSets, cmd);
                m_simpleRenderSystem->RenderObjects(data);

                UI::BeginUIFrame(cmd);

                objectDataWidget.Draw();

                UI::Debug_DrawMetrics(m_simpleRenderSystem->GetObjectsDrawn(), m_cam->GetPosition());

                UI::EndUIFrame(cmd);

                m_renderer->EndRendering(cmd);
                m_renderer->EndFrame();
            }
        }
    }
    m_logicalDevice->GetVkDevice().waitIdle();

    HGINFO("Quitting...");
}

} // namespace Humongous
