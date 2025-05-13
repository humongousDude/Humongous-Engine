#include "vulkan_app.hpp"
#include "allocator.hpp"
#include "audio_engine.hpp"
#include "camera.hpp"
#include "extra.hpp"
#include "gameobject.hpp"
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

    m_audioSource = ResourceManager::LoadAudioSource("song");
}

VulkanApp::~VulkanApp()
{
    m_logicalDevice->GetVkDevice().waitIdle();
    m_mainDeletionQueue.Flush();
}

void VulkanApp::Init(const int argc, char* argv[])
{
    InitializeLogging();

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

    AudioEngine::Init();

    m_renderer = std::make_unique<Renderer>(*m_window, *m_logicalDevice, *m_physicalDevice, m_logicalDevice->GetVmaAllocator(),
                                            vk::Format::eR16G16B16A16Sfloat, vk::Format::eD32Sfloat);

    m_cam = std::make_unique<Camera>(m_logicalDevice.get());

    std::vector<vk::DescriptorSetLayout> skyboxLayouts = {m_cam->GetDescriptorSetLayout()};

    ShaderSet set = {Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "simple.vert"),
                     Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "pbr.frag")};

    m_skyboxRenderSystem = std::make_unique<SkyboxRenderSystem>(m_logicalDevice.get(), "papermill", skyboxLayouts);

    std::vector<vk::DescriptorSetLayout> simpleLayouts = {m_cam->GetDescriptorSetLayout(), m_cam->GetParamDescriptorSetLayout(),
                                                          ResourceManager::GetSkyboxDescriptorLayout()};

    m_simpleRenderSystem = std::make_unique<SimpleRenderSystem>(*m_logicalDevice, simpleLayouts, set);

    m_mainDeletionQueue.PushDeletor([&]() {
        m_simpleRenderSystem.reset();
        m_skyboxRenderSystem.reset();
        m_renderer.reset();
        m_cam.reset();
        UI::Shutdown();
        ResourceManager::Shutdown();
        Allocator::Shutdown();
        AudioEngine::Shutdown();
        m_logicalDevice.reset();
        m_physicalDevice.reset();
        m_window.reset();
        m_instance.reset();
    });
}

void VulkanApp::LoadGameObjects()
{
    HGINFO("Loading game objects...");

    auto wallModel = ResourceManager::LoadModel("real_wall");

    GameObject wall = GameObject::CreateGameObject();
    wall.transform.translation = glm::vec3(0.0f, 0.0f, 50.f);
    wall.transform.scale = glm::vec3(10, 100, 10);
    wall.transform.rotation = glm::vec3(90, 0.0f, 0.0f);
    wall.SetModel(wallModel);
    wall.name = "Wall";
    m_gameObjects.emplace(wall.GetId(), std::move(wall));

    // Sponza isn't available by default. This tests the Asset Manager's ability to load a default model. However, if sponza is provided, it'll be
    // loaded instead
    // auto employeeModel = ResourceManager::LoadModel("Sponza");
    //
    // GameObject employee = GameObject::CreateGameObject();
    // employee.transform.translation = glm::vec3(5, 0, 0);
    // employee.transform.scale = glm::vec3(0.1f);
    // employee.transform.rotation = glm::vec3(0.0f);
    // employee.name = "Sponza";
    // employee.SetModel(employeeModel);

    // m_gameObjects.emplace(employee.GetId(), std::move(employee));

    auto damagedHelmetModel = ResourceManager::LoadModel("DamagedHelmet");

    GameObject car = GameObject::CreateGameObject();
    car.transform.translation = glm::vec3(-5, 0, 0);
    car.transform.scale = glm::vec3(5);
    car.name = "Damaged Helmet";
    car.SetModel(damagedHelmetModel);

    m_gameObjects.emplace(car.GetId(), std::move(car));

    auto employeeModel = ResourceManager::LoadModel("Sponza");
    n32  start = 100;
    n32  x, y, z = start;
    for(n32 i = 0; i < 100; ++i)
    {
        x++;
        if(x > start)
        {
            z++;
            x = start;
        }
        if(z > start)
        {
            y++;
            z = start;
        }

        GameObject mp = GameObject::CreateGameObject();
        mp.transform.translation = glm::vec3(x, y, z);
        mp.transform.scale = glm::vec3(0.1f);
        mp.transform.rotation = glm::vec3(0.0f);
        mp.name = "employeeStressTest";
        mp.SetModel(employeeModel);

        m_gameObjects.emplace(mp.GetId(), std::move(mp));
    }

    m_mainDeletionQueue.PushDeletor([&]() { m_gameObjects.clear(); });

    HGINFO("Loaded game objects");
}

void VulkanApp::HandleInput(const float frameTime, SDL_Event* event)
{
    float deltaX = 0.0f, deltaY = 0.0f;
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
        if(movementType == KeyboardHandler::Movements::FORWARD) { movementType = KeyboardHandler::Movements::NONE; }
        else { movementType = KeyboardHandler::Movements::BACKWARD; }
    }
    if(keyboardState[SDL_SCANCODE_A])
    {
        if(movementType == KeyboardHandler::Movements::RIGHT) { movementType = KeyboardHandler::Movements::NONE; }
        else if(movementType == KeyboardHandler::Movements::FORWARD) { movementType = KeyboardHandler::Movements::FORWARD_LEFT; }
        else if(movementType == KeyboardHandler::Movements::BACKWARD) { movementType = KeyboardHandler::Movements::BACKWARD_LEFT; }
        else { movementType = KeyboardHandler::Movements::LEFT; }
    }
    if(keyboardState[SDL_SCANCODE_D])
    {
        if(movementType == KeyboardHandler::Movements::LEFT) { movementType = KeyboardHandler::Movements::NONE; }
        else if(movementType == KeyboardHandler::Movements::FORWARD) { movementType = KeyboardHandler::Movements::FORWARD_RIGHT; }
        else if(movementType == KeyboardHandler::Movements::BACKWARD) { movementType = KeyboardHandler::Movements::BACKWARD_RIGHT; }
        else { movementType = KeyboardHandler::Movements::RIGHT; }
    }
    if(keyboardState[SDL_SCANCODE_Q])
    {
        if(movementType == KeyboardHandler::Movements::UP) { movementType = KeyboardHandler::Movements::NONE; }
        else { movementType = KeyboardHandler::Movements::DOWN; }
    }
    if(keyboardState[SDL_SCANCODE_E])
    {
        if(movementType == KeyboardHandler::Movements::DOWN) { movementType = KeyboardHandler::Movements::NONE; }
        else { movementType = KeyboardHandler::Movements::UP; }
    }
    if(keyboardState[SDL_SCANCODE_P]) { AudioEngine::PlaySound(m_audioSource); }

    const KeyboardHandler::InputData data{frameTime, movementType, deltaX, deltaY, *m_cam};

    handler.ProcessInput(data);
}

void VulkanApp::Run()
{
    m_cam->UpdateViewMatrix();

    auto currentTime = std::chrono::high_resolution_clock::now();

    UiWidget objectDataWidget{"Object Data", true, {0, 0}, {400, 500}, 0};
    for(auto& [id, obj]: m_gameObjects)
    {
        objectDataWidget.Add([&]() {
            ImGui::PushID(id);
            if(ImGui::CollapsingHeader(obj.name.c_str()))
            {
                ImGui::Text("ID: %i", id);

                float position[3] = {obj.transform.translation.x, obj.transform.translation.y, obj.transform.translation.z};
                ImGui::DragFloat3("Position", position, -100, 100);
                obj.transform.translation.x = position[0];
                obj.transform.translation.y = position[1];
                obj.transform.translation.z = position[2];

                float scale[3] = {obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z};
                ImGui::DragFloat3("scale", scale, -10, 10);
                obj.transform.scale.x = scale[0];
                obj.transform.scale.y = scale[1];
                obj.transform.scale.z = scale[2];

                float rotate[3] = {obj.transform.rotation.x, obj.transform.rotation.y, obj.transform.rotation.z};
                ImGui::DragFloat3("rotation", rotate, -360, 360);
                obj.transform.rotation.x = rotate[0];
                obj.transform.rotation.y = rotate[1];
                obj.transform.rotation.z = rotate[2];

                if(ImGui::TreeNode("Bounding box Corners"))
                {
                    for(n32 i = 0; i < obj.GetBoundingBox().corners.size(); ++i)
                    {
                        auto box = obj.GetBoundingBox();
                        ImGui::Text("Corner %i: %f, %f, %f", i, box.corners[i].x, box.corners[i].y, box.corners[i].z);
                    }

                    ImGui::TreePop();
                }
            }
            ImGui::PopID();
        });
    }

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
            m_cam->Update();

            auto sortedAndCulledObjs = Utils::SortAndCullGameObjects(*m_cam, m_gameObjects);
            auto sortedObjs = sortedAndCulledObjs;

            if(const auto cmd = m_renderer->BeginFrame(&sortedAndCulledObjs))
            {
                RenderData data{.commandBuffer = cmd,
                                .uboSets = {m_cam->GetDescriptorSet(m_renderer->GetFrameIndex())},
                                .sceneSets = {m_cam->GetParamDescriptorSet(m_renderer->GetFrameIndex())},
                                .skyboxSets = {m_skyboxRenderSystem->GetSkybox()->GetDescriptorSet()},
                                .gameObjects = &sortedAndCulledObjs,
                                .frameIndex = m_renderer->GetFrameIndex(),
                                .cam = *m_cam,
                                .renderer = *m_renderer,
                                .camPos = m_cam->GetPosition()};

                m_renderer->BeginDepthPrePass(cmd);

                m_simpleRenderSystem->RenderObjects(data, true);

                m_renderer->EndDepthPrePass(cmd);

                m_renderer->DoGPUOcclusionCulling(cmd, &sortedObjs, *m_cam);

                m_renderer->BeginRendering(cmd);

                m_skyboxRenderSystem->RenderSkybox(data.frameIndex, data.uboSets, cmd);
                m_simpleRenderSystem->RenderObjects(data, false);

                UI::BeginUIFrame(cmd);

                objectDataWidget.Draw();
                m_cam->DrawUI();

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
