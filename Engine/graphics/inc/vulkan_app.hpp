#pragma once

#include <array>

#include "asset_manager.hpp"
#include "deque"
#include "functional"
#include "instance.hpp"
#include "logical_device.hpp"
#include "memory"
#include "physical_device.hpp"
#include "render_graph.hpp"
#include "render_systems/simple_render_system.hpp"
#include "render_systems/skybox_render_system.hpp"
#include "renderer.hpp"
#include "window.hpp"

namespace Humongous
{
struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    void PushDeletor(std::function<void()> deletor) { deletors.push_front(deletor); }

    void Flush()
    {
        for(auto& deletor: deletors) { deletor(); }
        deletors.clear();
    }
};

class VulkanApp
{
public:
    VulkanApp(int argc, char* argv[]);
    ~VulkanApp();

    void Run();

private:
    DeletionQueue                                                                                  m_mainDeletionQueue;
    std::unique_ptr<Instance>                                                                      m_instance;
    std::unique_ptr<Window>                                                                        m_window;
    std::unique_ptr<PhysicalDevice>                                                                m_physicalDevice;
    std::unique_ptr<VulkanLogicalDevice>                                                           m_logicalDevice;
    std::unique_ptr<Renderer>                                                                      m_renderer;
    std::unique_ptr<Renderer>                                                                      m_uiRenderer;
    std::unique_ptr<IRenderSystem>                                                                 m_entityRenderSystem;
    std::unique_ptr<IRenderSystem>                                                                 m_depthRenderSystem;
    std::unique_ptr<SkyboxRenderSystem>                                                            m_skyboxRenderSystem;
    std::unique_ptr<Camera>                                                                        m_cam;
    std::unique_ptr<ResourceManager>                                                               m_resourceManager;
    std::unique_ptr<AssetManager>                                                                  m_assetManager;
    std::array<std::unique_ptr<RenderGraph>, static_cast<u32>(Globals::Limits::MaxFramesInFlight)> m_renderGraphs;

    void Init(int argc, char* argv[]);
    void CreateRenderSystems();
    void LoadGameObjects();

    void HandleInput(float frameTime, SDL_Event* event);
};
} // namespace Humongous
