// TIME LOST DUE TO VULKAN BUFFER / PUSH CONSTANT MISALIGNMENT: 10 HOURS

#include "allocator.hpp"
#include "camera.hpp"
#include "model.hpp"
#include <keyboard_handler.hpp>
#include <logger.hpp>
#include <thread>
#include <vulkan_app.hpp>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace Humongous
{

VulkanApp::VulkanApp()
{
    Init();
    LoadGameObjects();
    InitAudio();
}

VulkanApp::~VulkanApp() { m_mainDeletionQueue.Flush(); }

void VulkanApp::InitAudio()
{
    // OpenAL setup
    device = alcOpenDevice(nullptr);
    if(!device)
    {
        HGERROR("Failed to open OpenAL device");
        return;
    }

    context = alcCreateContext(device, nullptr);
    alcMakeContextCurrent(context);

    if(!context)
    {
        HGERROR("Failed to create OpenAL context");
        alcCloseDevice(device);
        return;
    }

    m_mainDeletionQueue.PushDeletor([&]() {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        alcCloseDevice(device);
    });
}

void VulkanApp::PlaySoundOnMainThread() { PlaySound(); }

void VulkanApp::PlaySoundOnDifferentThread()
{
    m_audioThread = std::thread{&VulkanApp::PlaySound, this};
    m_audioThread.detach();
}

void VulkanApp::PlaySound()
{
    // Open sound file using libsndfile
    const char* filename = "sfx/test.wav";
    SF_INFO     sfInfo;
    SNDFILE*    sndFile = sf_open(filename, SFM_READ, &sfInfo);
    if(!sndFile)
    {
        HGERROR("Failed to open sound file: %s", filename);
        return;
    }

    // Read sound data from file
    std::vector<ALshort> samples(sfInfo.frames * sfInfo.channels);
    sf_readf_short(sndFile, samples.data(), sfInfo.frames);

    // Close the file
    sf_close(sndFile);

    // Generate OpenAL buffer
    ALuint buffer;
    alGenBuffers(1, &buffer);
    checkALError("alGenBuffers");

    // Fill buffer with sound data
    alBufferData(buffer, (sfInfo.channels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, samples.data(), samples.size() * sizeof(ALshort),
                 sfInfo.samplerate);
    checkALError("alBufferData");

    // Generate OpenAL source
    ALuint source;
    alGenSources(1, &source);
    checkALError("alGenSources");

    // Attach buffer to source
    alSourcei(source, AL_BUFFER, buffer);
    checkALError("alSourcei");

    // Play the sound
    alSourcePlay(source);
    checkALError("alSourcePlay");

    // Wait for the sound to finish playing
    ALint sourceState;
    do {
        alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
    } while(sourceState == AL_PLAYING && !m_quit);

    alDeleteSources(1, &source);
    alDeleteBuffers(1, &buffer);
}

void VulkanApp::Init()
{
    m_window = std::make_unique<Window>();
    m_instance = std::make_unique<Instance>();
    m_physicalDevice = std::make_unique<PhysicalDevice>(*m_instance, *m_window);
    m_logicalDevice = std::make_unique<LogicalDevice>(*m_instance, *m_physicalDevice);

    Allocator::Get().Initialize(m_logicalDevice.get());

    m_renderer = std::make_unique<Renderer>(*m_window, *m_logicalDevice, *m_physicalDevice, m_logicalDevice->GetVmaAllocator());

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
    model = std::make_shared<Model>(m_logicalDevice.get(), "models/xeno_raven.glb", 0.1);

    GameObject obj = GameObject::CreateGameObject();
    obj.transform.translation = {0.0f, 0.0f, -1.0f};
    obj.transform.rotation = {glm::radians(180.0f), 0, 0};
    obj.transform.scale = {0.01f, 0.01f, 0.01f};

    obj.model = model;

    m_gameObjects.emplace(obj.GetId(), std::move(obj));

    std::shared_ptr<Model> employeeModel = std::make_unique<Model>(m_logicalDevice.get(), "models/employee.glb", 1);

    GameObject obj2 = GameObject::CreateGameObject();
    obj2.transform.translation = {2.0f, 0.0f, -1.0f};
    obj2.transform.rotation = {glm::radians(-90.f), 0, 0};
    obj2.transform.scale = {001.0f, 001.0f, 001.0f};

    obj2.model = employeeModel;

    m_gameObjects.emplace(obj2.GetId(), std::move(obj2));

    HGINFO("Loaded game objects");
    m_mainDeletionQueue.PushDeletor([&]() { m_gameObjects.clear(); });
}

void VulkanApp::Run()
{
    Camera cam{m_logicalDevice.get()};

    float aspect = m_renderer->GetAspectRatio();
    cam.SetViewTarget(glm::vec3(-1.0f, -2.0f, -2.0f), glm::vec3(0.0f, 0.0f, 2.5f));

    std::vector<VkDescriptorSetLayout> simpleLayouts = {cam.GetDescriptorSetLayout(), cam.GetParamDescriptorSetLayout()};
    std::vector<VkDescriptorSetLayout> skyboxLayouts = {cam.GetDescriptorSetLayout()};

    m_simpleRenderSystem = std::make_unique<SimpleRenderSystem>(*m_logicalDevice, simpleLayouts);
    m_skyboxRenderSystem = std::make_unique<SkyboxRenderSystem>(m_logicalDevice.get(), "textures/papermill.ktx", skyboxLayouts);

    GameObject viewerObject = GameObject::CreateGameObject();
    viewerObject.transform.translation.z = -2.5f;
    cam.SetViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

    KeyboardHandler handler{};

    PlaySoundOnDifferentThread();

    HGINFO("Running...");
    while(!m_window->ShouldWindowClose())
    {
        glfwPollEvents();

        aspect = m_renderer->GetAspectRatio();

        handler.MoveInPlaneXZ(m_window->GetWindow(), 0.01f, viewerObject);
        cam.SetViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

        cam.SetPerspectiveProjection(glm::radians(60.0f), aspect, 0.1f, 1000.0f);

        if(!m_window->IsMinimized() && m_window->IsFocused())
        {
            if(auto cmd = m_renderer->BeginFrame())
            {
                RenderData data{.commandBuffer = cmd,
                                .uboSets = {cam.GetDescriptorSet(m_renderer->GetFrameIndex())},
                                .sceneSets = {cam.GetParamDescriptorSet(m_renderer->GetFrameIndex())},
                                .gameObjects = m_gameObjects,
                                .frameIndex = m_renderer->GetFrameIndex()};

                cam.UpdateUBO(m_renderer->GetFrameIndex(), viewerObject.transform.translation);

                m_renderer->BeginRendering(cmd);

                m_skyboxRenderSystem->RenderSkybox(data.frameIndex, data.uboSets, cmd);
                m_simpleRenderSystem->RenderObjects(data);

                m_renderer->EndRendering(cmd);
                m_renderer->EndFrame();
            }
        }
    }
    m_quit = true;
    vkDeviceWaitIdle(m_logicalDevice->GetVkDevice());

    HGINFO("Quitting...");
}

} // namespace Humongous
