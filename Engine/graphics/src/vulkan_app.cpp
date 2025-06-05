#define VMA_IMPLEMENTATION

#include "vulkan_app.hpp"
#include "allocator.hpp"
#include "asset_manager.hpp"
#include "audio_engine.hpp"
#include "camera.hpp"
#include "globals.hpp"
#include "imgui_impl_sdl3.h"
#include "keyboard_handler.hpp"
#include "logger.hpp"
#include "resource_manager.hpp"
#include "scene_handler.hpp"
#include "ui/ui.hpp"
#include "vk_mem_alloc.h"

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

    SceneHandler::Init();

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
        AudioEngine::Shutdown();
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

    auto world = SceneHandler::GetWorld();

    // auto house = world->CreateEntity();
    // world->AddComponent<BoundingBox>(house);
    // world->AddComponent<ModelComponent>(house);
    // auto comp = world->GetComponent<ModelComponent>(house);
    // comp->modelHandle = ResourceManager::RequestModel("wow");

    auto helmet = world->CreateEntity();
    world->AddComponent<BoundingBox>(helmet);
    world->AddComponent<ModelComponent>(helmet);
    auto comp = world->GetComponent<ModelComponent>(helmet);
    comp->modelHandle = ResourceManager::RequestModel("DamagedHelmet");
    world->AddComponent<AudioSourceComponent>(helmet, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::AUDIO, "default"));

    auto wall = world->CreateEntity();
    world->AddComponent<BoundingBox>(wall);
    world->AddComponent<ModelComponent>(wall);
    comp = world->GetComponent<ModelComponent>(wall);
    comp->modelHandle = ResourceManager::RequestModel("real_wall");
    world->AddComponent<AudioSourceComponent>(wall, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::AUDIO, "default"));

    auto transform = world->GetComponent<TransformComponent>(wall);
    transform->SetTranslation(0, 0, 10);
    transform->SetRotation(90, 0, 0);
    transform->SetScale(1, 10, 1);

    s32 x, y, z = 0;
    for(n32 i = 0; i < 1000; ++i)
    {
        x += 1;

        if(x > 10)
        {
            x = 0;
            z += 1;
        }
        if(z > 10)
        {
            z = 0;
            y += 1;
        }

        auto employee = world->CreateEntity();
        world->AddComponent<BoundingBox>(employee);
        world->AddComponent<ModelComponent>(employee);
        comp = world->GetComponent<ModelComponent>(employee);
        comp->modelHandle = ResourceManager::RequestModel("wow");
        world->AddComponent<AudioSourceComponent>(employee, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::AUDIO, "default"));

        transform = world->GetComponent<TransformComponent>(employee);
        transform->SetTranslation(00, 0, 5);
        transform->SetScale(2.f, 2.f, 2.f);
    }

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
    if(keyboardState[SDL_SCANCODE_P])
    {
        auto world = SceneHandler::GetWorld();
        for(const auto& entityId: world->GetComponentStorage<AudioSourceComponent>().GetDense())
        {
            auto audio = world->GetComponent<AudioSourceComponent>(entityId);
            AudioEngine::Play(*audio);
        }
    }

    const KeyboardHandler::InputData data{frameTime, movementType, deltaX, deltaY, *m_cam};

    handler.ProcessInput(data);
}

void VulkanApp::Run()
{
    m_cam->UpdateViewMatrix();

    auto currentTime = std::chrono::high_resolution_clock::now();

    auto world = SceneHandler::GetWorld();
    world->BoundingVolumeUpdateSystem();

    UiWidget objectDataWidget{"Object Data", true, {0, 0}, {400, 500}, 0};
    objectDataWidget.Add([&]() {
        for(n32 entityId = 0; entityId < world->GetComponentStorage<TransformComponent>().GetDense().size(); entityId++)
        {
            BoundingBox* bb = world->GetComponent<BoundingBox>(entityId);

            if(!bb)
            {
                HGINFO("We somehow don't have a bounding box?");
                continue;
            }
            if(!bb->valid)
            {

                HGINFO("We somehow don't have a valid bounding box?");
                continue;
            }

            TransformComponent* transform = world->GetComponent<TransformComponent>(entityId);
            if(!transform) { continue; }

            ImGui::PushID(entityId);

            if(ImGui::CollapsingHeader("entityId"))
            {
                ImGui::Text("ID: %i", entityId);

                float position[3] = {transform->GetTranslation().x, transform->GetTranslation().y, transform->GetTranslation().z};
                ImGui::DragFloat3("Position", position);
                transform->SetTranslation(position[0], position[1], position[2]);

                float scale[3] = {transform->GetScale().x, transform->GetScale().y, transform->GetScale().z};
                ImGui::DragFloat3("scale", scale);
                transform->SetScale(scale[0], scale[1], scale[2]);

                float rotate[3] = {transform->GetRotation().x, transform->GetRotation().y, transform->GetRotation().z};
                ImGui::DragFloat3("rotation", rotate);
                transform->SetRotation(rotate[0], rotate[1], rotate[2]);

                if(ImGui::TreeNode("Bounding box Corners"))
                {
                    for(n32 i = 0; i < bb->corners.size(); ++i)
                    {
                        ImGui::Text("Corner %i: %f, %f, %f", i, bb->corners[i].x, bb->corners[i].y, bb->corners[i].z);
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::PopID();
        }
    });

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
            m_cam->Update();
            AudioEngine::UpdateListener(m_cam->GetPosition(), {0, 0, 0}, m_cam->GetForward(), m_cam->GetUp());

            auto world = SceneHandler::GetWorld();
            world->BoundingVolumeUpdateSystem();
            AudioEngine::UpdateSources();

            auto frustumCulledEntities = Utils::SortAndCullEntities(*m_cam, *world);
            auto sortedObjs = frustumCulledEntities;

            const auto cmd = m_renderer->BeginFrame(frustumCulledEntities);
            if(cmd != VK_NULL_HANDLE)
            {
                RenderData data{
                    .commandBuffer = cmd,
                    .uboSets = {m_cam->GetDescriptorSet(m_renderer->GetFrameIndex())},
                    .sceneSets = {m_cam->GetParamDescriptorSet(m_renderer->GetFrameIndex())},
                    .skyboxSets = {m_skyboxRenderSystem->GetSkybox()->GetDescriptorSet()},
                    .visibleEntities = &frustumCulledEntities,
                    .world = *world,
                    .frameIndex = m_renderer->GetFrameIndex(),
                    .cam = *m_cam,
                };

                data.visibleEntities = &sortedObjs;
                m_renderer->BeginDepthPrePass(cmd);

                m_simpleRenderSystem->RenderObjects(data, true);

                m_renderer->EndDepthPrePass(cmd);

                data.visibleEntities = &frustumCulledEntities;

                m_renderer->DoGPUOcclusionCulling(cmd, sortedObjs, *world, *m_cam);

                m_renderer->BeginGeometryPass(cmd);

                m_simpleRenderSystem->RenderObjects(data, false);

                m_renderer->EndGeometryPass(cmd);

                m_renderer->DoLightingPass(cmd, m_cam->GetFragmentDescriptorSet(m_renderer->GetFrameIndex()),
                                           m_cam->GetParamDescriptorSet(m_renderer->GetFrameIndex()),
                                           m_skyboxRenderSystem->GetSkybox()->GetDescriptorSet());

                m_renderer->BeginSkyboxPass(cmd);
                m_skyboxRenderSystem->RenderSkybox(data.frameIndex, data.uboSets, cmd);
                m_renderer->EndSkyboxPass(cmd);

                m_renderer->BeginUIRendering(cmd);

                UI::BeginUIFrame(cmd);

                objectDataWidget.Draw();
                m_cam->DrawUI();

                UI::Debug_DrawMetrics(m_simpleRenderSystem->GetObjectsDrawn(), m_cam->GetPosition());

                UI::EndUIFrame(cmd);
                m_renderer->EndUIRendering(cmd);

                m_renderer->EndFrame();
            }
        }
    }
    m_logicalDevice->GetVkDevice().waitIdle();

    HGINFO("Quitting...");
}

} // namespace Humongous
