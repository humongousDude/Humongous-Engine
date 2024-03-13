#pragma once

#include "render_systems/simple_render_system.hpp"
#include "render_systems/skybox_render_system.hpp"
#include "window.hpp"
#include <deque>
#include <functional>
#include <instance.hpp>
#include <logical_device.hpp>
#include <memory>
#include <physical_device.hpp>
#include <swapchain.hpp>

#include "logger.hpp"
#include <AL/al.h>
#include <AL/alc.h>
#include <sndfile.h>
#include <thread>

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
    VulkanApp();
    ~VulkanApp();

    void Run();

private:
    DeletionQueue m_mainDeletionQueue;

    std::unique_ptr<Instance>           m_instance;
    std::unique_ptr<Window>             m_window;
    std::unique_ptr<PhysicalDevice>     m_physicalDevice;
    std::unique_ptr<LogicalDevice>      m_logicalDevice;
    std::unique_ptr<Renderer>           m_renderer;
    std::unique_ptr<SimpleRenderSystem> m_simpleRenderSystem;
    std::unique_ptr<SkyboxRenderSystem> m_skyboxRenderSystem;
    std::thread                         m_audioThread;

    bool m_quit = false;

    GameObject::Map m_gameObjects;

    void Init();
    void LoadGameObjects();

    ALCcontext* context;
    ALCdevice*  device;

    void InitAudio();

    void PlaySoundOnDifferentThread();
    void PlaySoundOnMainThread();

    // called from above 2 funcs, 1 with diff thread other on main thread
    void PlaySound();

    void checkALError(const char* message)
    {
        ALenum error = alGetError();
        if(error != AL_NO_ERROR) { HGERROR("OpenAL error ( %s ): %s", message, alGetString(error)); }
    }
};
} // namespace Humongous
