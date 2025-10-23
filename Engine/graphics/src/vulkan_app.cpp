#include "vulkan_app.hpp"
#include "audio_engine.hpp"
#include "camera.hpp"
#include "chrono"
#include "extra.hpp"
#include "globals.hpp"
#include "imgui_impl_sdl3.h"
#include "keyboard_handler.hpp"
#include "logger.hpp"
#include "scene_handler.hpp"
#include "ui/ui.hpp"

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

void VulkanApp::Init(const int argc, char* argv[])
{
    InitializeLogging();

    m_window = std::make_unique<Window>();
    m_instance = std::make_unique<Instance>();
    m_physicalDevice = std::make_unique<PhysicalDevice>(*m_instance, *m_window);
    m_logicalDevice = std::make_unique<VulkanLogicalDevice>(*m_instance, *m_physicalDevice);

    if(argc > 1)
    {
        std::vector<std::string> paths;
        for(int i = 1; i < argc; ++i) { paths.emplace_back(argv[i]); }
        m_assetManager = std::make_unique<AssetManager>(&paths);
    }
    else
    {
        HGINFO("Launch the engine with absolute paths to extra directories for the asset manager to look for models in. %s");
        m_assetManager = std::make_unique<AssetManager>();
    }

    m_resourceManager = std::make_unique<ResourceManager>(*m_logicalDevice, *m_assetManager);

    UI::Init(*m_instance, *m_logicalDevice, *m_window);

    AudioEngine::Init();

    SceneHandler::Init();

    m_renderer = std::make_unique<Renderer>(*m_window, *m_logicalDevice, *m_physicalDevice, *m_resourceManager, *m_assetManager);

    m_cam = std::make_unique<Camera>(*m_logicalDevice);

    std::vector<vk::DescriptorSetLayout> skyboxLayouts = {m_cam->GetVertexDescriptorLayout()};

    m_skyboxRenderSystem = std::make_unique<SkyboxRenderSystem>(*m_logicalDevice, *m_resourceManager, *m_assetManager, skyboxLayouts);

    CreateRenderSystems();

    for(auto& rg: m_renderGraphs) { rg = std::make_unique<RenderGraph>(*m_logicalDevice); }
    m_mainDeletionQueue.PushDeletor([&]() {
        m_entityRenderSystem.reset();
        m_skyboxRenderSystem.reset();
        m_renderer.reset();
        m_cam.reset();
        UI::Shutdown();
        m_resourceManager.reset();
        AudioEngine::Shutdown();
        // Allocator::Shutdown();
        m_logicalDevice.reset();
        m_physicalDevice.reset();
        m_window.reset();
        m_instance.reset();
    });
}

void VulkanApp::CreateRenderSystems()
{
    HGINFO("Creating entity render system...");
    const auto&                          resourceLayouts = m_resourceManager->GetDescriptorLayouts();
    std::vector<vk::DescriptorSetLayout> simpleLayouts;
    simpleLayouts.push_back(m_cam->GetVertexDescriptorLayout());
    simpleLayouts.insert(simpleLayouts.end(), resourceLayouts.begin(), resourceLayouts.end());

    RenderPipeline::PipelineConfigInfo configInfo = RenderPipeline::DefaultPipelineConfigInfo();

    configInfo.vertShaderPath = m_assetManager->GetAsset(AssetManager::AssetType::SHADER, "simple.vert");
    configInfo.fragShaderPath = m_assetManager->GetAsset(AssetManager::AssetType::SHADER, "pbr.frag");
    configInfo.meshShaderPath = m_assetManager->GetAsset(AssetManager::AssetType::SHADER, "simple.mesh");
    configInfo.taskShaderPath = m_assetManager->GetAsset(AssetManager::AssetType::SHADER, "simple.task");
    configInfo.descriptorSetLayouts = simpleLayouts;

    configInfo.rasterizationInfo.cullMode = vk::CullModeFlagBits::eBack;

    configInfo.colorBlendAttachment.blendEnable = false;
    configInfo.colorAttachmentFormat = vk::Format::eR8G8B8A8Unorm;

    configInfo.colorBlendAttachments.clear();
    configInfo.colorBlendAttachments.push_back(configInfo.colorBlendAttachment);
    configInfo.colorBlendAttachments.push_back(configInfo.colorBlendAttachment);
    configInfo.colorBlendAttachments.push_back(configInfo.colorBlendAttachment);

    configInfo.colorAttachmentFormats.clear();
    configInfo.colorAttachmentFormats.push_back(configInfo.colorAttachmentFormat);
    configInfo.colorAttachmentFormats.push_back(configInfo.colorAttachmentFormat);
    configInfo.colorAttachmentFormats.push_back(configInfo.colorAttachmentFormat);
    configInfo.colorBlendInfo.attachmentCount = configInfo.colorBlendAttachments.size();
    configInfo.renderingInfo.colorAttachmentCount = configInfo.colorBlendAttachments.size();
    configInfo.renderingInfo.pColorAttachmentFormats = configInfo.colorAttachmentFormats.data();
    configInfo.colorBlendInfo.pAttachments = configInfo.colorBlendAttachments.data();

    configInfo.renderingInfo.depthAttachmentFormat = vk::Format::eD32SfloatS8Uint;
    configInfo.colorBlendInfo.logicOpEnable = false;
    configInfo.depthStencilInfo.depthCompareOp = vk::CompareOp::eEqual;
    configInfo.depthStencilInfo.depthWriteEnable = false;
    configInfo.depthStencilInfo.stencilTestEnable = true;

    vk::StencilOpState stencilState{};
    stencilState.compareOp = vk::CompareOp::eAlways;
    stencilState.passOp = vk::StencilOp::eReplace;
    stencilState.reference = static_cast<u32>(Globals::StencilMasks::Model);
    stencilState.compareMask = 0xFF;
    stencilState.writeMask = 0xFF;
    configInfo.depthStencilInfo.front = stencilState;
    configInfo.depthStencilInfo.back = stencilState;
    configInfo.renderingInfo.stencilAttachmentFormat = vk::Format::eD32SfloatS8Uint;

    configInfo.useMeshShaders = m_logicalDevice->GetPhysicalDevice().GetCurrentCapabilities().supportsMeshShaders;

    if(configInfo.useMeshShaders)
    {
        m_entityRenderSystem = std::make_unique<MeshRenderSystem>(*m_logicalDevice, *m_resourceManager, *m_assetManager, configInfo);
    }
    else
    {
        m_entityRenderSystem = std::make_unique<TraditionalRenderSystem>(*m_logicalDevice, *m_resourceManager, *m_assetManager, configInfo);
    }
    HGINFO("Created entity render system");

    HGINFO("Creating depth only render system...");

    configInfo.colorAttachmentFormat = vk::Format::eUndefined;
    configInfo.colorAttachmentFormats.clear();
    configInfo.colorBlendAttachments.clear();
    configInfo.colorBlendInfo.attachmentCount = configInfo.colorBlendAttachments.size();
    configInfo.renderingInfo.colorAttachmentCount = configInfo.colorBlendAttachments.size();
    configInfo.renderingInfo.pColorAttachmentFormats = configInfo.colorAttachmentFormats.data();
    configInfo.colorBlendInfo.pAttachments = configInfo.colorBlendAttachments.data();

    configInfo.depthStencilInfo.depthCompareOp = vk::CompareOp::eGreaterOrEqual;
    configInfo.depthStencilInfo.depthTestEnable = true;
    configInfo.depthStencilInfo.depthWriteEnable = true;
    configInfo.depthStencilInfo.stencilTestEnable = false;

    configInfo.renderingInfo.stencilAttachmentFormat = vk::Format::eUndefined;

    configInfo.depthStencilInfo.front = vk::StencilOpState{};
    configInfo.depthStencilInfo.front = vk::StencilOpState{};

    configInfo.useFragmentShader = false;

    if(configInfo.useMeshShaders)
    {
        m_depthRenderSystem = std::make_unique<MeshRenderSystem>(*m_logicalDevice, *m_resourceManager, *m_assetManager, configInfo);
    }
    else
    {
        m_depthRenderSystem = std::make_unique<TraditionalRenderSystem>(*m_logicalDevice, *m_resourceManager, *m_assetManager, configInfo);
    }
    HGINFO("Created depth only render system");

    m_mainDeletionQueue.PushDeletor([&]() {
        m_entityRenderSystem.reset();
        m_depthRenderSystem.reset();
    });
}

void VulkanApp::LoadGameObjects()
{
    HGINFO("Loading game objects...");

    auto world = SceneHandler::GetWorld();

    auto house = world->CreateEntity();
    world->AddComponent<ModelComponent>(house);
    auto comp = world->GetComponent<ModelComponent>(house);
    comp->instance = m_resourceManager->RequestModel("buster_drone");
    std::string name = comp->instance->GetModel()->GetName();

    auto transform = world->GetComponent<TransformComponent>(house);
    transform->SetScale(1.0, 1.0, 1.0);
    transform->SetRotation(0, 0, 0);
    transform->SetTranslation(0, 0, 10);
    world->GetComponent<NameComponent>(house)->name = name + std::to_string(house);

    auto helmet = world->CreateEntity();
    world->AddComponent<BoundingBox>(helmet);
    world->AddComponent<ModelComponent>(helmet);
    comp = world->GetComponent<ModelComponent>(helmet);
    comp->instance = m_resourceManager->RequestModel("buster_drone");
    world->AddComponent<AudioSourceComponent>(helmet, m_assetManager->GetAsset(AssetManager::AssetType::AUDIO, "default"));
    name = comp->instance->GetModel()->GetName();
    world->GetComponent<NameComponent>(helmet)->name = name + std::to_string(helmet);

    transform = world->GetComponent<TransformComponent>(helmet);
    transform->SetTranslation(5, 0, 10);
    transform->SetScale(1, 1, 1);
    //
    auto drone = world->CreateEntity();
    world->AddComponent<BoundingBox>(drone);
    world->AddComponent<ModelComponent>(drone);
    comp = world->GetComponent<ModelComponent>(drone);
    comp->instance = m_resourceManager->RequestModel("CommercialRefrigerator");
    world->AddComponent<AudioSourceComponent>(drone, m_assetManager->GetAsset(AssetManager::AssetType::AUDIO, "default"));
    name = comp->instance->GetModel()->GetName();
    world->GetComponent<NameComponent>(drone)->name = name + std::to_string(helmet);

    transform = world->GetComponent<TransformComponent>(drone);
    transform->SetTranslation(10, 10, -20);
    transform->SetScale(1, 1, 1);

    f32 start = 0;
    f32 end = 100;
    f32 step = 2.5;
    f32 border = 15;
    f32 x = start, y = start, z = start;
    for(u32 i = 0; i < end; i++)
    {
        x += step;

        if(x > border)
        {
            x = start;
            z += step;
        }
        if(z > border)
        {
            z = start;
            y += step;
        }

        auto model = world->CreateEntity();
        world->AddComponent<ModelComponent>(model);
        auto comp = world->GetComponent<ModelComponent>(model);
        comp->instance = m_resourceManager->RequestModel("buster_drone");

        auto transform = world->GetComponent<TransformComponent>(model);
        transform->SetTranslation(x, y, z);
        std::string name = comp->instance->GetModel()->GetName();
        world->GetComponent<NameComponent>(model)->name = name + std::to_string(model);
    }

    HGINFO("Loaded game objects");
}

void VulkanApp::HandleInput(const f32 frameTime)
{
    float deltaX = 0.0f, deltaY = 0.0f;
    auto  movementType = KeyboardHandler::Movements::NONE;

    KeyboardHandler handler;

    const bool* keyboardState = SDL_GetKeyboardState(nullptr);

    SDL_GetRelativeMouseState(&deltaX, &deltaY);

    if(keyboardState[SDL_SCANCODE_I] && !m_window->IsCursorHidden())
    {
        m_window->HideCursor();
        deltaX = 0;
        deltaY = 0;
    }
    if((keyboardState[SDL_SCANCODE_O] || keyboardState[SDL_SCANCODE_ESCAPE]) && m_window->IsCursorHidden())
    {
        m_window->ShowCursor();
        deltaX = 0;
        deltaY = 0;
    }
    if(!m_window->IsCursorHidden()) { return; }

    if(keyboardState[SDL_SCANCODE_W]) { movementType = KeyboardHandler::Movements::FORWARD; }
    if(keyboardState[SDL_SCANCODE_S])
    {
        if(movementType == KeyboardHandler::Movements::FORWARD) { movementType = KeyboardHandler::Movements::NONE; }
        else
        {
            movementType = KeyboardHandler::Movements::BACKWARD;
        }
    }
    if(keyboardState[SDL_SCANCODE_A])
    {
        if(movementType == KeyboardHandler::Movements::RIGHT) { movementType = KeyboardHandler::Movements::NONE; }
        else if(movementType == KeyboardHandler::Movements::FORWARD) { movementType = KeyboardHandler::Movements::FORWARD_LEFT; }
        else if(movementType == KeyboardHandler::Movements::BACKWARD) { movementType = KeyboardHandler::Movements::BACKWARD_LEFT; }
        else
        {
            movementType = KeyboardHandler::Movements::LEFT;
        }
    }
    if(keyboardState[SDL_SCANCODE_D])
    {
        if(movementType == KeyboardHandler::Movements::LEFT) { movementType = KeyboardHandler::Movements::NONE; }
        else if(movementType == KeyboardHandler::Movements::FORWARD) { movementType = KeyboardHandler::Movements::FORWARD_RIGHT; }
        else if(movementType == KeyboardHandler::Movements::BACKWARD) { movementType = KeyboardHandler::Movements::BACKWARD_RIGHT; }
        else
        {
            movementType = KeyboardHandler::Movements::RIGHT;
        }
    }
    if(keyboardState[SDL_SCANCODE_Q])
    {
        if(movementType == KeyboardHandler::Movements::UP) { movementType = KeyboardHandler::Movements::NONE; }
        else
        {
            movementType = KeyboardHandler::Movements::DOWN;
        }
    }
    if(keyboardState[SDL_SCANCODE_E])
    {
        if(movementType == KeyboardHandler::Movements::DOWN) { movementType = KeyboardHandler::Movements::NONE; }
        else
        {
            movementType = KeyboardHandler::Movements::UP;
        }
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
    auto currentTime = std::chrono::high_resolution_clock::now();

    auto world = SceneHandler::GetWorld();
    world->BoundingVolumeUpdateSystem();

    UiWidget objectDataWidget{"Object Data", true, {0, 0}, {400, 500}, 0};
    objectDataWidget.Add([&]() {
        for(u32 entityId = 0; entityId < world->GetComponentStorage<TransformComponent>().GetDense().size(); entityId++)
        {
            TransformComponent* transform = world->GetComponent<TransformComponent>(entityId);

            ImGui::PushID(entityId);

            if(ImGui::CollapsingHeader(world->GetComponent<NameComponent>(entityId)->name.c_str()))
            {
                ImGui::Text("ID: %i", entityId);

                float position[3] = {transform->GetTranslation().x(), transform->GetTranslation().y(), transform->GetTranslation().z()};
                ImGui::DragFloat3("Position", position);
                transform->SetTranslation(position[0], position[1], position[2]);

                float scale[3] = {transform->GetScale().x(), transform->GetScale().y(), transform->GetScale().z()};
                ImGui::DragFloat3("scale", scale);
                transform->SetScale(scale[0], scale[1], scale[2]);

                float rotate[3] = {transform->GetRotation().x(), transform->GetRotation().y(), transform->GetRotation().z()};
                ImGui::DragFloat3("rotation", rotate);
                transform->SetRotation(rotate[0], rotate[1], rotate[2]);

                BoundingBox* bb = world->GetComponent<BoundingBox>(entityId);

                if(bb && bb->valid)
                {
                    if(ImGui::TreeNode("Bounding box corners"))
                    {
                        std::array<Eigen::Vector4f, 8> corners;
                        corners[0] = Eigen::Vector4f(bb->min.x(), bb->min.y(), bb->min.z(), 1.0f);
                        corners[1] = Eigen::Vector4f(bb->max.x(), bb->min.y(), bb->min.z(), 1.0f);
                        corners[2] = Eigen::Vector4f(bb->min.x(), bb->max.y(), bb->min.z(), 1.0f);
                        corners[3] = Eigen::Vector4f(bb->max.x(), bb->max.y(), bb->min.z(), 1.0f);
                        corners[4] = Eigen::Vector4f(bb->min.x(), bb->min.y(), bb->max.z(), 1.0f);
                        corners[5] = Eigen::Vector4f(bb->max.x(), bb->min.y(), bb->max.z(), 1.0f);
                        corners[6] = Eigen::Vector4f(bb->min.x(), bb->max.y(), bb->max.z(), 1.0f);
                        corners[7] = Eigen::Vector4f(bb->max.x(), bb->max.y(), bb->max.z(), 1.0f);
                        for(u32 i = 0; i < corners.size(); ++i)
                        {
                            ImGui::Text("Corner %i: %f, %f, %f", i, corners[i].x(), corners[i].y(), corners[i].z());
                        }

                        ImGui::TreePop();
                    }
                }

                auto instance = world->GetComponent<ModelComponent>(entityId)->instance;

                if(instance->GetModel()->HasAnimations())
                {
                    u32                     size = world->GetComponentStorage<ModelComponent>().GetDense().size();
                    static std::vector<u32> itemSelectedIndex;
                    itemSelectedIndex.resize(size);
                    const char* preview = instance->GetAnimations()[itemSelectedIndex[entityId]].name.c_str();
                    if(ImGui::BeginCombo("Animations", preview))
                    {
                        for(u32 i = 0; i < static_cast<u32>(instance->GetAnimations().size()); i++)
                        {
                            const b8 isSelected = (itemSelectedIndex[entityId] == i);
                            if(ImGui::Selectable(instance->GetAnimations()[i].name.c_str(), isSelected)) { itemSelectedIndex[entityId] = i; }

                            if(isSelected) { ImGui::SetItemDefaultFocus(); }
                        }
                        instance->SetAnimation(instance->GetAnimations()[itemSelectedIndex[entityId]].name);
                        ImGui::EndCombo();
                    }

                    for(u32 i = 0; i < instance->GetMorphCount(); ++i) { ImGui::Text("Morph %i at %f", i, instance->GetMorph(i)); }

                    if(ImGui::Button("Play Animation")) { instance->PlayAnimation(); }
                    if(ImGui::Button("Stop Animation")) { instance->StopAnimation(); }
                    if(ImGui::Button("Pause Animation")) { instance->PauseAnimation(); }
                    if(ImGui::Button("UnPause Animation")) { instance->UnPauseAnimation(); }
                }
            }

            ImGui::PopID();
        }
    });

    HGINFO("Running...");
    b8        quit = false;
    b8        focused = false;
    b8        minimized = false;
    SDL_Event e;
    while(!quit)
    {
        auto       newTime = std::chrono::high_resolution_clock::now();
        const auto frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;
        Globals::Time::Update(frameTime);

        while(SDL_PollEvent(&e))
        {
            if(e.type == SDL_EVENT_QUIT)
            {
                quit = true;
                break;
            }
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
        if(quit) { break; }

        HandleInput(frameTime);

        const float aspect = m_renderer->GetAspectRatio();

        m_cam->SetPerspectiveProjection(Utils::DegreesToRadians(80.0f), aspect, 0.1f, 1000.0f);

        if(!minimized && focused)
        {
            m_cam->Update();

            AudioEngine::UpdateListener(m_cam->GetPosition(), {0, 0, 0}, m_cam->GetForward(), m_cam->GetUp());

            world->BoundingVolumeUpdateSystem();

            AudioEngine::UpdateSources();

            auto frustumAndSortedEntities = Utils::SortAndCullEntities(*m_cam, *world);

            auto sortedObjs = frustumAndSortedEntities;

            auto world = SceneHandler::GetWorld();
            world->ModelInstanceUpdateSystem(*m_resourceManager);

            m_resourceManager->FinalizeGPUData();

            const auto cmd = m_renderer->BeginFrame(frustumAndSortedEntities);

            if(cmd != VK_NULL_HANDLE)
            {
                IRenderSystem::RenderData data{
                    .commandBuffer = cmd,
                    .uboSets = {m_cam->GetVertexDescriptorSet(m_renderer->GetFrameIndex())},
                    .entities = &sortedObjs,
                    .world = *world,
                    .frameIndex = m_renderer->GetFrameIndex(),
                    .cam = *m_cam,
                };
                m_depthRenderSystem->ReadyBuffers(data);
                m_entityRenderSystem->ReadyBuffers(data);
                m_depthRenderSystem->ReadyDescriptors(data);
                m_entityRenderSystem->ReadyDescriptors(data);

                std::function<void(IRenderSystem::RenderData data)> depthExec = [&](const IRenderSystem::RenderData& data) {
                    m_renderer->BeginDepthPrePass(data.commandBuffer);
                    m_depthRenderSystem->Render(data);
                    m_renderer->EndDepthPrePass(data.commandBuffer);
                };

                data.entities = &frustumAndSortedEntities;
                std::function<void(const IRenderSystem::RenderData& data)> mainExec = [&](const IRenderSystem::RenderData& data) {
                    m_renderer->BeginGeometryPass(data.commandBuffer);
                    m_entityRenderSystem->Render(data);
                    m_renderer->EndGeometryPass(data.commandBuffer);
                };

                auto geometryPass = m_renderGraphs[m_renderer->GetFrameIndex()]->AddPass("Main Render Pass", {}, mainExec);
                auto depthPass = m_renderGraphs[m_renderer->GetFrameIndex()]->AddPass("Depth Pre-Pass", {}, depthExec);

                geometryPass->AddDependency(depthPass);

                std::function<void(const IRenderSystem::RenderData& data)> lightExec = [&](const IRenderSystem::RenderData& data) {
                    m_renderer->DoLightingPass(data.commandBuffer, m_cam->GetComputeDescriptorSet(m_renderer->GetFrameIndex()),
                                               m_cam->GetParamDescriptorSet(m_renderer->GetFrameIndex()),
                                               m_skyboxRenderSystem->GetSkybox()->GetCompDescriptorSet());
                };

                auto lightingPass = m_renderGraphs[m_renderer->GetFrameIndex()]->AddPass("Lighting pass", {geometryPass}, lightExec);
                std::function<void(const IRenderSystem::RenderData& data)> skyboxExec = [&](const IRenderSystem::RenderData& data) {
                    m_renderer->BeginSkyboxPass(cmd);

                    m_skyboxRenderSystem->RenderSkybox(data.uboSets, data.commandBuffer);

                    m_renderer->EndSkyboxPass(cmd);
                };
                auto skyboxPass = m_renderGraphs[m_renderer->GetFrameIndex()]->AddPass("Skybox pass", {lightingPass}, skyboxExec);

                std::function<void(const IRenderSystem::RenderData& data)> uiExec = [&](const IRenderSystem::RenderData& data) {
                    m_renderer->BeginUIPass(data.commandBuffer);

                    UI::BeginUIFrame();

                    objectDataWidget.Draw();
                    m_cam->DrawUI();

                    UI::Debug_DrawMetrics(0, m_cam->GetPosition());

                    UI::EndUIFrame(data.commandBuffer);

                    m_renderer->EndUIPass(data.commandBuffer);

                    ImGui::UpdatePlatformWindows();
                    ImGui::RenderPlatformWindowsDefault();
                };

                m_renderGraphs[m_renderer->GetFrameIndex()]->AddPass("UI Pass", {geometryPass, skyboxPass}, uiExec);

                m_renderGraphs[m_renderer->GetFrameIndex()]->Compile();
                m_renderGraphs[m_renderer->GetFrameIndex()]->Execute(data);
            }

            m_renderer->EndFrame();
        }
    }
    m_logicalDevice->GetVkDevice().waitIdle();

    HGINFO("Quitting...");
}

} // namespace Humongous
