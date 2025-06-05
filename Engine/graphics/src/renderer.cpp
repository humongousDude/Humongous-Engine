#include "renderer.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "extra.hpp"
#include "images.hpp"
#include "logger.hpp"
#include "render_pipeline.hpp"
#include "render_systems/simple_render_system.hpp"
#include "scene_handler.hpp"
#include <array>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

namespace Humongous
{

Renderer::Renderer(Window& window, LogicalDevice& logicalDevice, PhysicalDevice& physicalDevice, VmaAllocator allocator, vk::Format drawFormat,
                   vk::Format depthFormat)
    : m_window{window}, m_logicalDevice{logicalDevice}, m_physicalDevice{physicalDevice}, m_allocator{allocator}
{
    m_drawImage.imageFormat = drawFormat;
    m_depthImage.imageFormat = depthFormat;

    CreateCommandPool();
    AllocateCommandBuffers();
    InitSyncStructures();
    InitLightingPipeline();
    RecreateSwapChain();
    CreateComputePipeline();
}

Renderer::~Renderer()
{
    HGINFO("Destroying renderer...");
    if(m_commandPool) { m_logicalDevice.GetVkDevice().destroyCommandPool(m_commandPool, nullptr); }

    for(Frame& frame: m_frames)
    {
        m_logicalDevice.GetVkDevice().destroySemaphore(frame.imageAvailableSemaphore, nullptr);
        m_logicalDevice.GetVkDevice().destroyFence(frame.inFlightFence, nullptr);

        frame.gbuffer.albedo.Destroy(m_logicalDevice);
        frame.gbuffer.normalRough.Destroy(m_logicalDevice);
        frame.gbuffer.materialParam.Destroy(m_logicalDevice);
        frame.gbuffer.position.Destroy(m_logicalDevice);
        frame.gbuffer.depth.Destroy(m_logicalDevice);
    }

    if(m_drawImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, m_drawImage.image, m_drawImage.allocation); }
    if(m_depthImage.imageView != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroyImageView(m_drawImage.imageView, nullptr); }
    if(m_depthImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, m_depthImage.image, m_depthImage.allocation); }
    if(m_depthImage.imageView != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroyImageView(m_depthImage.imageView, nullptr); }
    if(m_depthImageSampler != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroySampler(m_depthImageSampler); }
    if(m_debugImageSampler != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroySampler(m_debugImageSampler); }

    m_logicalDevice.GetVkDevice().destroyPipeline(m_computePipeline, nullptr);
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(m_computePipelineLayout, nullptr);

    m_lightingPipeline.reset();
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(m_lightingPipelineLayout, nullptr);

    m_lightingLayout.reset();
    m_lightingPool.reset();

    m_computeLayout.reset();
    m_computePool.reset();

    m_swapChain.reset();
    HGINFO("Destroyed renderer");
}

void Renderer::RecreateSwapChain()
{
    HGINFO("Recreating swap chain...");
    m_logicalDevice.GetVkDevice().waitIdle();

    auto extent = m_window.GetExtent();
    while(extent.width == 0 || extent.height == 0) { extent = m_window.GetExtent(); }

    if(m_swapChain == nullptr) { m_swapChain = std::make_unique<SwapChain>(m_window, m_physicalDevice, m_logicalDevice); }
    else
    {
        std::shared_ptr<SwapChain> oldSwapChain = std::move(m_swapChain);
        m_swapChain = std::make_unique<SwapChain>(m_window, m_physicalDevice, m_logicalDevice, std::move(m_swapChain));
    }
    // recreate the image views

    HGINFO("Recreated swap chain");

    InitImagesAndViews();
    InitDepthImage();
    InitGBuffer();
}

void Renderer::InitImagesAndViews()
{
    if(m_drawImage.imageView != VK_NULL_HANDLE) { vkDestroyImageView(m_logicalDevice.GetVkDevice(), m_drawImage.imageView, nullptr); }
    if(m_drawImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, m_drawImage.image, m_drawImage.allocation); }

    HGINFO("Creating draw image and view...");

    vk::Extent3D drawImageExtent = {m_swapChain->GetExtent().width, m_swapChain->GetExtent().height, 1};

    m_drawImage.imageExtent = drawImageExtent;

    vk::ImageUsageFlags drawImageUsages{};
    drawImageUsages |= vk::ImageUsageFlagBits::eTransferSrc;
    drawImageUsages |= vk::ImageUsageFlagBits::eTransferDst;
    drawImageUsages |= vk::ImageUsageFlagBits::eStorage;
    drawImageUsages |= vk::ImageUsageFlagBits::eColorAttachment;

    Utils::AllocatedImageCreateInfo imgCI{.logicalDevice = m_logicalDevice, .allocatedImage = &m_drawImage};
    imgCI.layerCount = 1;
    imgCI.imageViewType = vk::ImageViewType::e2D;
    imgCI.tiling = vk::ImageTiling::eOptimal;
    imgCI.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imgCI.aspectFlags = vk::ImageAspectFlagBits::eColor;
    imgCI.height = m_drawImage.imageExtent.height;
    imgCI.width = m_drawImage.imageExtent.width;
    imgCI.mipLevels = 1;
    imgCI.usage = drawImageUsages;
    imgCI.layerCount = 1;
    imgCI.format = m_drawImage.imageFormat == vk::Format::eUndefined ? vk::Format::eR16G16B16A16Sfloat : m_drawImage.imageFormat;
    imgCI.imagePool = VK_NULL_HANDLE;
    imgCI.samples = vk::SampleCountFlagBits::e1;

    Utils::CreateAllocatedImage(imgCI);

    HGINFO("Created draw image and view");
}

void Renderer::InitDepthImage()
{
    if(m_depthImage.imageView != VK_NULL_HANDLE) { vkDestroyImageView(m_logicalDevice.GetVkDevice(), m_depthImage.imageView, nullptr); }
    if(m_depthImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, m_depthImage.image, m_depthImage.allocation); }

    HGINFO("Creating depth image and view...");

    vk::Extent3D depthImageExtent = {m_swapChain->GetExtent().width, m_swapChain->GetExtent().height, 1};

    // hardcoding the draw format to 32 bit float
    // m_depthImage.imageFormat = vk::Format::eD32Sfloat;
    m_depthImage.imageExtent = depthImageExtent;

    vk::ImageUsageFlags depthImageUsages{};
    depthImageUsages |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    depthImageUsages |= vk::ImageUsageFlagBits::eSampled;

    Utils::AllocatedImageCreateInfo imgCI{.logicalDevice = m_logicalDevice, .allocatedImage = &m_depthImage};
    imgCI.layerCount = 1;
    // imgCI.flags = 0;
    imgCI.imageViewType = vk::ImageViewType::e2D;
    imgCI.tiling = vk::ImageTiling::eOptimal;
    imgCI.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imgCI.aspectFlags = vk::ImageAspectFlagBits::eDepth;
    imgCI.height = m_depthImage.imageExtent.height;
    imgCI.width = m_depthImage.imageExtent.width;
    imgCI.mipLevels = 1;
    imgCI.usage = depthImageUsages;
    imgCI.format = vk::Format::eD32Sfloat;
    imgCI.imagePool = VK_NULL_HANDLE;
    imgCI.samples = vk::SampleCountFlagBits::e1;

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();

    Utils::CreateAllocatedImage(imgCI);

    Utils::ImageTransitionInfo postComputeDepthTransition{cmd,
                                                          vk::ImageLayout::eUndefined,
                                                          vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                                          &m_logicalDevice,
                                                          m_depthImage.image,
                                                          vk::ImageAspectFlagBits::eDepth};

    Utils::TransitionImageLayout(postComputeDepthTransition);

    m_logicalDevice.EndSingleTimeCommands(cmd);

    HGINFO("Created depth image and view");
}

void Renderer::InitGBuffer()
{
    HGINFO("Initializing G-Buffer...");
    vk::ImageUsageFlags drawImageUsages{};
    drawImageUsages |= vk::ImageUsageFlagBits::eColorAttachment;
    drawImageUsages |= vk::ImageUsageFlagBits::eSampled;

    Utils::SamplerCreateInfo samCI{};
    samCI.magFilter = vk::Filter::eLinear;
    samCI.minFilter = vk::Filter::eLinear;
    samCI.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samCI.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samCI.addressModeW = vk::SamplerAddressMode::eClampToEdge;

    Utils::AllocatedImageCreateInfo imgCI{.logicalDevice = m_logicalDevice};
    imgCI.layerCount = 1;
    imgCI.imageViewType = vk::ImageViewType::e2D;
    imgCI.tiling = vk::ImageTiling::eOptimal;
    imgCI.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imgCI.aspectFlags = vk::ImageAspectFlagBits::eColor;
    imgCI.width = m_swapChain->GetExtent().width;
    imgCI.height = m_swapChain->GetExtent().height;
    imgCI.mipLevels = 1;
    imgCI.usage = drawImageUsages;
    imgCI.layerCount = 1;
    imgCI.format = vk::Format::eR16G16B16A16Sfloat;
    imgCI.imagePool = VK_NULL_HANDLE;
    imgCI.samples = vk::SampleCountFlagBits::e1;
    imgCI.createWithSampler = true;
    imgCI.samplerInfo = &samCI;
    imgCI.initialLayout = vk::ImageLayout::eUndefined;

    if(!m_lightingPool)
    {
        DescriptorPool::Builder builder{m_logicalDevice};
        builder.AddPoolSize(vk::DescriptorType::eCombinedImageSampler, 45);
        builder.SetMaxSets(45);
        m_lightingPool = builder.Build();
    }
    else { m_lightingPool->ResetPool(); }

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();
    for(int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
    {
        if(m_frames[i].gbuffer.albedo.image != VK_NULL_HANDLE) { m_frames[i].gbuffer.albedo.Destroy(m_logicalDevice); }
        if(m_frames[i].gbuffer.normalRough.image != VK_NULL_HANDLE) { m_frames[i].gbuffer.normalRough.Destroy(m_logicalDevice); }
        if(m_frames[i].gbuffer.materialParam.image != VK_NULL_HANDLE) { m_frames[i].gbuffer.materialParam.Destroy(m_logicalDevice); }
        if(m_frames[i].gbuffer.position.image != VK_NULL_HANDLE) { m_frames[i].gbuffer.position.Destroy(m_logicalDevice); }
        if(m_frames[i].gbuffer.depth.image != VK_NULL_HANDLE) { m_frames[i].gbuffer.depth.Destroy(m_logicalDevice); }

        imgCI.allocatedImage = &m_frames[i].gbuffer.albedo;
        imgCI.usage = drawImageUsages;
        imgCI.format = vk::Format::eR16G16B16A16Sfloat;
        imgCI.aspectFlags = vk::ImageAspectFlagBits::eColor;
        Utils::CreateAllocatedImage(imgCI);

        imgCI.allocatedImage = &m_frames[i].gbuffer.normalRough;
        Utils::CreateAllocatedImage(imgCI);

        imgCI.allocatedImage = &m_frames[i].gbuffer.materialParam;
        Utils::CreateAllocatedImage(imgCI);

        imgCI.allocatedImage = &m_frames[i].gbuffer.position;
        Utils::CreateAllocatedImage(imgCI);

        imgCI.allocatedImage = &m_frames[i].gbuffer.depth;
        imgCI.aspectFlags = vk::ImageAspectFlagBits::eDepth;
        imgCI.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
        imgCI.format = vk::Format::eD32Sfloat;
        Utils::CreateAllocatedImage(imgCI);

        m_lightingPool->AllocateDescriptor(m_lightingLayout->GetDescriptorSetLayout(), m_frames[i].gbuffer.imageSet);
    }
    m_logicalDevice.EndSingleTimeCommands(cmd);
    HGINFO("Initialized G-Buffer");
}

void Renderer::InitLightingPipeline()
{
    HGINFO("Creating pipeline layout...");
    DescriptorSetLayout::Builder builder{m_logicalDevice};
    builder.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
    builder.AddBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
    builder.AddBinding(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
    builder.AddBinding(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
    builder.AddBinding(4, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
    m_lightingLayout = builder.Build();

    DescriptorSetLayout::Builder scene{m_logicalDevice};
    scene.AddBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1);
    auto layout = scene.Build();

    DescriptorSetLayout::Builder cam{m_logicalDevice};
    cam.AddBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1);
    auto camlayout = cam.Build();

    vk::PushConstantRange indexRange{};
    indexRange.stageFlags = vk::ShaderStageFlagBits::eFragment;
    indexRange.offset = sizeof(Model::PushConstantData);
    indexRange.size = sizeof(n32);

    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
    descriptorSetLayouts.push_back(camlayout->GetDescriptorSetLayout());
    descriptorSetLayouts.push_back(layout->GetDescriptorSetLayout());
    descriptorSetLayouts.push_back(ResourceManager::GetSkyboxDescriptorLayout());
    descriptorSetLayouts.push_back(m_lightingLayout->GetDescriptorSetLayout());

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<n32>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if(m_logicalDevice.GetVkDevice().createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_lightingPipelineLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create pipeline layout");
    }

    HGINFO("Created pipeline layout");

    HGINFO("Creating pipeline...");

    ShaderSet shaderSet{Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "lighting.vert"),
                        Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "lighting.frag")};

    RenderPipeline::PipelineConfigInfo configInfo = RenderPipeline::DefaultPipelineConfigInfo();
    configInfo.pipelineLayout = m_lightingPipelineLayout;
    configInfo.vertShaderPath = shaderSet.vertShaderPath;
    configInfo.fragShaderPath = shaderSet.fragShaderPath;

    configInfo.renderingInfo.colorAttachmentCount = 1;
    auto format = vk::Format::eR16G16B16A16Sfloat;
    configInfo.renderingInfo.pColorAttachmentFormats = &format;
    configInfo.renderingInfo.depthAttachmentFormat = vk::Format::eUndefined;

    m_lightingPipeline = std::make_unique<RenderPipeline>(m_logicalDevice, configInfo);

    HGINFO("Created pipeline");
}

void Renderer::CreateCommandPool()
{
    HGINFO("Creating command pool...");
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = m_physicalDevice.FindQueueFamilies(m_physicalDevice.GetVkPhysicalDevice()).graphicsFamily.value();

    if(m_logicalDevice.GetVkDevice().createCommandPool(&poolInfo, nullptr, &m_commandPool) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create command pool");
    }

    HGINFO("Created command pool");
}

void Renderer::AllocateCommandBuffers()
{
    HGINFO("Allocating command buffers...");

    m_frames.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = static_cast<n32>(m_frames.size());

    for(Frame& frame: m_frames)
    {
        if(m_logicalDevice.GetVkDevice().allocateCommandBuffers(&allocInfo, &frame.commandBuffer) != vk::Result::eSuccess)
        {
            HGERROR("Failed to allocate command buffers");
        }
    }

    HGINFO("Allocated command buffers");
}

void Renderer::InitSyncStructures()
{
    HGINFO("Initializing synchronization structures...");

    // one fence to control when the gpu has finished rendering the frame
    // and 2 semaphores to synchronize rendering with swapchain
    vk::FenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    vk::SemaphoreCreateInfo semaphoreCreateInfo{};

    for(int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
    {
        if(m_logicalDevice.GetVkDevice().createFence(&fenceCreateInfo, nullptr, &m_frames[i].inFlightFence) != vk::Result::eSuccess)
        {
            HGERROR("Failed to create fence");
        }

        if(m_logicalDevice.GetVkDevice().createSemaphore(&semaphoreCreateInfo, nullptr, &m_frames[i].imageAvailableSemaphore) !=
           vk::Result::eSuccess)
        {
            HGERROR("Failed to create image available semaphore");
        }
    }

    HGINFO("Initialized synchronization structures");
}

void Renderer::CreateComputePipeline()
{
    HGINFO("Creating compute pipeline...");
    DescriptorPool::Builder builder{m_logicalDevice};
    builder.AddPoolSize(vk::DescriptorType::eCombinedImageSampler, 5);
    builder.AddPoolSize(vk::DescriptorType::eStorageBuffer, 5);
    builder.AddPoolSize(vk::DescriptorType::eUniformBuffer, 5);
    builder.AddPoolSize(vk::DescriptorType::eStorageImage, 5);
    builder.SetMaxSets(100);
    m_computePool = builder.Build();

    DescriptorSetLayout::Builder builder2{m_logicalDevice};
    builder2.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(3, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(4, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(5, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute);
    m_computeLayout = builder2.Build();

    vk::SamplerCreateInfo samplerInfo{};
    // samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = vk::Filter::eNearest;
    samplerInfo.minFilter = vk::Filter::eNearest;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = vk::CompareOp::eNever;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0;
    samplerInfo.compareEnable = VK_FALSE;

    if(m_logicalDevice.GetVkDevice().createSampler(&samplerInfo, nullptr, &m_depthImageSampler) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create texture sampler");
    }

    auto layout = m_computeLayout->GetDescriptorSetLayout();

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &layout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if(m_logicalDevice.GetVkDevice().createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_computePipelineLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create pipeline layout");
    }

    auto compCode = Utils::ReadFile(Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "occlusion.comp"));

    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = compCode.size();
    createInfo.pCode = reinterpret_cast<const n32*>(compCode.data());

    vk::ShaderModule compModule;

    if(m_logicalDevice.GetVkDevice().createShaderModule(&createInfo, nullptr, &compModule) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create shader module!");
    }

    vk::PipelineShaderStageCreateInfo why{};
    why.stage = vk::ShaderStageFlagBits::eCompute;
    why.pName = "main";
    why.module = compModule;

    vk::ComputePipelineCreateInfo compInfo{};
    compInfo.layout = m_computePipelineLayout;
    compInfo.stage = why;

    if(m_logicalDevice.GetVkDevice().createComputePipelines(nullptr, 1, &compInfo, nullptr, &m_computePipeline) != vk::Result::eSuccess)
    {
        HGFATAL("Failed to create renderer compute pipeline!");
    }

    vkDestroyShaderModule(m_logicalDevice.GetVkDevice(), compModule, nullptr);
    HGINFO("Created compute pipeline");
}

void Renderer::ReadyPerFrameData(std::vector<Utils::VisibleEntityInfo>& visibleEntities)
{
    auto world = SceneHandler::GetWorld();

    auto&    visibilityResultsBuffer = GetCurrentFrame().visibilityResults;
    uint32_t numObjectsCulledLastFrame = GetCurrentFrame().numObjectsDispatched;

    if(!visibilityResultsBuffer || numObjectsCulledLastFrame == 0) { return; }

    if(visibilityResultsBuffer->GetBufferSize() < numObjectsCulledLastFrame * sizeof(VisiblityResultSet))
    {
        HGERROR("Visibility results buffer size mismatch for frame %u! Expected at least %zu bytes, buffer is %zu. VisiblityResultSet size: %zu",
                m_currentFrameIndex, numObjectsCulledLastFrame * sizeof(VisiblityResultSet), visibilityResultsBuffer->GetBufferSize(),
                sizeof(VisiblityResultSet));
        return;
    }

    visibilityResultsBuffer->Map();
    VisiblityResultSet* results = static_cast<VisiblityResultSet*>(visibilityResultsBuffer->GetMappedMemory());

    std::unordered_map<EntityID, bool> prevFrameVisibilityByEntityId;
    prevFrameVisibilityByEntityId.reserve(numObjectsCulledLastFrame);

    for(uint32_t i = 0; i < numObjectsCulledLastFrame; ++i)
    {
        EntityID entityId = results[i].id;
        bool     isVisible = results[i].visible;

        prevFrameVisibilityByEntityId[entityId] = isVisible;
    }

    visibilityResultsBuffer->UnMap();

    std::vector<Utils::VisibleEntityInfo> filteredVisibleEntities;
    filteredVisibleEntities.reserve(visibleEntities.size());

    for(const auto& visibleEntityInfo: visibleEntities)
    {
        EntityID entityId = visibleEntityInfo.id;

        auto it = prevFrameVisibilityByEntityId.find(entityId);

        bool wasVisibleLastFrame = true;

        if(it != prevFrameVisibilityByEntityId.end()) { wasVisibleLastFrame = it->second; }
        // else { wasVisibleLastFrame = true; }

        if(wasVisibleLastFrame) { filteredVisibleEntities.push_back(visibleEntityInfo); }
    }

    visibleEntities = std::move(filteredVisibleEntities);
}

void Renderer::PreRenderTransitions(vk::CommandBuffer cmd)
{
    m_drawImageExtent.width = m_drawImage.imageExtent.width;
    m_drawImageExtent.height = m_drawImage.imageExtent.height;
    m_depthImageExtent.width = m_depthImage.imageExtent.width;
    m_depthImageExtent.height = m_depthImage.imageExtent.height;

    {
        Utils::ImageTransitionInfo drawInfo{};
        drawInfo.image = GetCurrentFrame().gbuffer.albedo.image;
        drawInfo.oldLayout = vk::ImageLayout::eUndefined;
        drawInfo.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        drawInfo.imageAspect = vk::ImageAspectFlagBits::eColor;
        drawInfo.cmd = cmd;
        GetCurrentFrame().gbuffer.albedo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        Utils::ImageTransitionInfo drawInfo{};
        drawInfo.image = GetCurrentFrame().gbuffer.normalRough.image;
        drawInfo.oldLayout = vk::ImageLayout::eUndefined;
        drawInfo.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        drawInfo.imageAspect = vk::ImageAspectFlagBits::eColor;
        drawInfo.cmd = cmd;
        GetCurrentFrame().gbuffer.normalRough.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        Utils::ImageTransitionInfo drawInfo{};
        drawInfo.image = GetCurrentFrame().gbuffer.materialParam.image;
        drawInfo.oldLayout = vk::ImageLayout::eUndefined;
        drawInfo.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        drawInfo.imageAspect = vk::ImageAspectFlagBits::eColor;
        drawInfo.cmd = cmd;
        GetCurrentFrame().gbuffer.materialParam.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        Utils::ImageTransitionInfo drawInfo{};
        drawInfo.image = GetCurrentFrame().gbuffer.position.image;
        drawInfo.oldLayout = vk::ImageLayout::eUndefined;
        drawInfo.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        drawInfo.imageAspect = vk::ImageAspectFlagBits::eColor;
        drawInfo.cmd = cmd;
        GetCurrentFrame().gbuffer.position.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        Utils::ImageTransitionInfo drawInfo{};
        drawInfo.image = GetCurrentFrame().gbuffer.depth.image;
        drawInfo.oldLayout = vk::ImageLayout::eUndefined;
        drawInfo.newLayout = vk::ImageLayout::eDepthAttachmentOptimal;
        drawInfo.cmd = cmd;
        drawInfo.imageAspect = vk::ImageAspectFlagBits::eDepth;
        GetCurrentFrame().gbuffer.depth.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
}

void Renderer::PostRenderTransitions(vk::CommandBuffer cmd)
{
    {
        Utils::ImageTransitionInfo drawInfo{};
        drawInfo.image = GetCurrentFrame().gbuffer.albedo.image;
        drawInfo.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        drawInfo.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        drawInfo.cmd = cmd;
        GetCurrentFrame().gbuffer.albedo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        Utils::ImageTransitionInfo drawInfo{};
        drawInfo.image = GetCurrentFrame().gbuffer.normalRough.image;
        drawInfo.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        drawInfo.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        drawInfo.cmd = cmd;
        GetCurrentFrame().gbuffer.normalRough.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        Utils::ImageTransitionInfo drawInfo{};
        drawInfo.image = GetCurrentFrame().gbuffer.materialParam.image;
        drawInfo.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        drawInfo.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        drawInfo.cmd = cmd;
        GetCurrentFrame().gbuffer.materialParam.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        Utils::ImageTransitionInfo drawInfo{};
        drawInfo.image = GetCurrentFrame().gbuffer.position.image;
        drawInfo.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        drawInfo.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        drawInfo.cmd = cmd;
        GetCurrentFrame().gbuffer.position.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        Utils::ImageTransitionInfo drawInfo{};
        drawInfo.image = GetCurrentFrame().gbuffer.depth.image;
        drawInfo.oldLayout = vk::ImageLayout::eDepthAttachmentOptimal;
        drawInfo.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        drawInfo.cmd = cmd;
        drawInfo.imageAspect = vk::ImageAspectFlagBits::eDepth;
        GetCurrentFrame().gbuffer.depth.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
}

vk::CommandBuffer Renderer::BeginFrame(std::vector<Utils::VisibleEntityInfo>& visibleEntities)
{
    vk::Result result = m_logicalDevice.GetVkDevice().waitForFences(1, &GetCurrentFrame().inFlightFence, vk::True, std::numeric_limits<n64>::max());
    if(result != vk::Result::eSuccess) { HGINFO("Failed to wait for fences: %s", vk::to_string(result).c_str()); }

    result = m_logicalDevice.GetVkDevice().resetFences(1, &GetCurrentFrame().inFlightFence);
    if(result != vk::Result::eSuccess) { HGINFO("Failed to reset fences: %s", vk::to_string(result).c_str()); }

    result = m_swapChain->AcquireNextImage(GetCurrentFrame().imageAvailableSemaphore, m_currentImageIndex);
    if(result != vk::Result::eSuccess) { HGINFO("Failed to acquire swapchain image: %s", vk::to_string(result).c_str()); }

    if(result == vk::Result::eErrorOutOfDateKHR)
    {
        RecreateSwapChain();
        return VK_NULL_HANDLE;
    }

    if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        HGERROR("failed to acquire swap chain image!");
        return VK_NULL_HANDLE;
    }

    ReadyPerFrameData(visibleEntities);

    auto                       cmd = m_logicalDevice.BeginSingleTimeCommands();
    Utils::ImageTransitionInfo transInfo{};
    transInfo.image = m_drawImage.image;
    transInfo.oldLayout = vk::ImageLayout::eUndefined;
    transInfo.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    transInfo.cmd = cmd;
    transInfo.imageAspect = vk::ImageAspectFlagBits::eColor;

    Utils::TransitionImageLayout(transInfo);
    m_logicalDevice.EndSingleTimeCommands(cmd);

    cmd = GetCurrentFrame().commandBuffer;
    cmd.reset();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

    if(cmd.begin(&beginInfo) != vk::Result::eSuccess) { HGERROR("Failed to begin recording command buffer"); }

    return cmd;
}

void Renderer::EndFrame()
{
    auto cmd = GetCurrentFrame().commandBuffer;

    Utils::ImageTransitionInfo drawInfo{};
    drawInfo.image = m_drawImage.image;
    drawInfo.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    drawInfo.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    drawInfo.cmd = cmd;
    Utils::TransitionImageLayout(drawInfo);

    Utils::ImageTransitionInfo swapInfo{};
    swapInfo.image = m_swapChain->GetImages()[m_currentImageIndex];
    swapInfo.oldLayout = vk::ImageLayout::eUndefined;
    swapInfo.newLayout = vk::ImageLayout::eTransferDstOptimal;
    swapInfo.cmd = cmd;

    Utils::TransitionImageLayout(swapInfo);

    Utils::CopyImageToImage(cmd, m_drawImage.image, m_swapChain->GetImages()[m_currentImageIndex], m_drawImageExtent, m_swapChain->GetExtent());

    Utils::ImageTransitionInfo presentTransitionInfo{};
    presentTransitionInfo.image = m_swapChain->GetImages()[m_currentImageIndex];
    presentTransitionInfo.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    presentTransitionInfo.newLayout = vk::ImageLayout::ePresentSrcKHR;
    presentTransitionInfo.cmd = cmd;

    Utils::TransitionImageLayout(presentTransitionInfo);

    cmd.end();

    vk::CommandBufferSubmitInfo cmdInfo{};
    cmdInfo.deviceMask = 0;
    cmdInfo.setCommandBuffer(GetCurrentFrame().commandBuffer);

    vk::SemaphoreSubmitInfo waitInfo{};
    waitInfo.semaphore = GetCurrentFrame().imageAvailableSemaphore;
    waitInfo.stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;

    vk::SemaphoreSubmitInfo signalInfo{};
    signalInfo.semaphore = m_swapChain->GetRenderFinishedSemaphoreAtIndex(m_currentImageIndex);
    signalInfo.stageMask = vk::PipelineStageFlagBits2::eAllGraphics;

    vk::SubmitInfo2 submit{};
    submit.waitSemaphoreInfoCount = 1;
    submit.pWaitSemaphoreInfos = &waitInfo;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signalInfo;

    vk::Result result = m_logicalDevice.GetGraphicsQueue().submit2(1, &submit, GetCurrentFrame().inFlightFence);

    if(result != vk::Result::eSuccess) { HGERROR("Failed to submit command buffer"); }

    auto s = m_swapChain->GetSwapChain();

    vk::PresentInfoKHR presentInfo{};
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &s;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &m_swapChain->GetRenderFinishedSemaphoreAtIndex(m_currentImageIndex);
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &m_currentImageIndex;

    result = m_logicalDevice.GetPresentQueue().presentKHR(&presentInfo);
    if(result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_window.WasWindowResized())
    {
        m_window.ResetWindowResizedFlag();
        RecreateSwapChain();
    }

    else if(result != vk::Result::eSuccess) { HGERROR("failed to present swap chain image"); }

    m_currentFrameIndex = (m_currentFrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::BeginGeometryPass(vk::CommandBuffer cmd)
{
    PreRenderTransitions(cmd);
    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    colorAttachment.imageView = m_drawImage.imageView;
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = clearValues[0];

    std::array<vk::RenderingAttachmentInfo, 4> colorAttachments{colorAttachment, colorAttachment, colorAttachment, colorAttachment};
    colorAttachments[0].imageView = GetCurrentFrame().gbuffer.albedo.imageView;

    colorAttachments[1].imageView = GetCurrentFrame().gbuffer.normalRough.imageView;

    colorAttachments[2].imageView = GetCurrentFrame().gbuffer.materialParam.imageView;

    colorAttachments[3].imageView = GetCurrentFrame().gbuffer.position.imageView;

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    depthAttachment.imageView = GetCurrentFrame().gbuffer.depth.imageView;
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue = clearValues[1];

    vk::RenderingInfo renderingInfo{};
    renderingInfo.sType = vk::StructureType::eRenderingInfo;
    renderingInfo.renderArea = {0, 0, m_swapChain->GetExtent().width, m_swapChain->GetExtent().height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    // renderingInfo.flags = 0;

    cmd.beginRendering(&renderingInfo);

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChain->GetExtent().width);
    viewport.height = static_cast<float>(m_swapChain->GetExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    cmd.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent.width = m_drawImageExtent.width;
    scissor.extent.height = m_drawImageExtent.height;

    cmd.setScissor(0, 1, &scissor);
}

void Renderer::EndGeometryPass(vk::CommandBuffer cmd)
{
    vkCmdEndRendering(cmd);
    PostRenderTransitions(cmd);
}

void Renderer::DoLightingPass(vk::CommandBuffer cmd, vk::DescriptorSet camSet, vk::DescriptorSet sceneSet, vk::DescriptorSet skyboxSet)
{
    vk::ClearValue clearValue;
    clearValue.color = vk::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f);

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    colorAttachment.imageView = m_drawImage.imageView;
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = clearValue;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.sType = vk::StructureType::eRenderingInfo;
    renderingInfo.renderArea = {0, 0, m_swapChain->GetExtent().width, m_swapChain->GetExtent().height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    // renderingInfo.flags = 0;

    cmd.beginRendering(&renderingInfo);

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_drawImageExtent.width);
    viewport.height = static_cast<float>(m_drawImageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    cmd.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent.width = m_drawImageExtent.width;
    scissor.extent.height = m_drawImageExtent.height;

    cmd.setScissor(0, 1, &scissor);

    auto albedoInfo = GetCurrentFrame().gbuffer.albedo.GetDescriptorInfo();
    auto normalInfo = GetCurrentFrame().gbuffer.normalRough.GetDescriptorInfo();
    auto matInfo = GetCurrentFrame().gbuffer.materialParam.GetDescriptorInfo();
    auto depthInfo = GetCurrentFrame().gbuffer.position.GetDescriptorInfo();
    auto posInfo = GetCurrentFrame().gbuffer.depth.GetDescriptorInfo();

    DescriptorWriter writer(*m_lightingLayout, m_lightingPool.get());
    writer.WriteImage(0, &albedoInfo);
    writer.WriteImage(1, &normalInfo);
    writer.WriteImage(2, &matInfo);
    writer.WriteImage(3, &posInfo);
    writer.WriteImage(4, &depthInfo);

    if(GetCurrentFrame().gbuffer.imageSet == VK_NULL_HANDLE) { writer.Build(GetCurrentFrame().gbuffer.imageSet); }
    else { writer.Overwrite(GetCurrentFrame().gbuffer.imageSet); }

    m_lightingPipeline->Bind(cmd);

    std::vector<vk::DescriptorSet> sets = {camSet, sceneSet, skyboxSet, GetCurrentFrame().gbuffer.imageSet};
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_lightingPipelineLayout, 0, sets.size(), sets.data(), 0, nullptr);

    cmd.draw(3, 1, 0, 0);

    cmd.endRendering();
}

void Renderer::BeginSkyboxPass(vk::CommandBuffer cmd)
{
    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    colorAttachment.imageView = m_drawImage.imageView;
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = clearValues[0];

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    depthAttachment.imageView = m_depthImage.imageView;
    depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue = clearValues[1];

    vk::RenderingInfo renderingInfo{};
    renderingInfo.sType = vk::StructureType::eRenderingInfo;
    renderingInfo.renderArea = {0, 0, m_swapChain->GetExtent().width, m_swapChain->GetExtent().height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;

    cmd.beginRendering(&renderingInfo);

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_drawImageExtent.width);
    viewport.height = static_cast<float>(m_drawImageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    cmd.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent.width = m_drawImageExtent.width;
    scissor.extent.height = m_drawImageExtent.height;

    cmd.setScissor(0, 1, &scissor);
}

void Renderer::EndSkyboxPass(vk::CommandBuffer cmd) { cmd.endRendering(); }

void Renderer::BeginUIRendering(vk::CommandBuffer cmd)
{
    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    colorAttachment.imageView = m_drawImage.imageView;
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = clearValues[0];

    vk::RenderingInfo renderingInfo{};
    renderingInfo.sType = vk::StructureType::eRenderingInfo;
    renderingInfo.renderArea = {0, 0, m_swapChain->GetExtent().width, m_swapChain->GetExtent().height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    // renderingInfo.flags = 0;

    cmd.beginRendering(&renderingInfo);
}

void Renderer::EndUIRendering(vk::CommandBuffer cmd) { cmd.endRendering(); }

void Renderer::BeginDepthPrePass(vk::CommandBuffer cmd)
{
    m_drawImageExtent.width = m_drawImage.imageExtent.width;
    m_drawImageExtent.height = m_drawImage.imageExtent.height;
    m_depthImageExtent.width = m_depthImage.imageExtent.width;
    m_depthImageExtent.height = m_depthImage.imageExtent.height;

    vk::ClearValue clearValue{};
    clearValue.depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    depthAttachment.imageView = m_depthImage.imageView;
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue = clearValue;

    vk::RenderingInfo renderingInfo{};
    // renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {0, 0, m_swapChain->GetExtent().width, m_swapChain->GetExtent().height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 0;
    renderingInfo.pColorAttachments = nullptr;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    // renderingInfo.flags = 0;

    renderingInfo.pDepthAttachment = &depthAttachment;

    cmd.beginRendering(&renderingInfo);

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_drawImageExtent.width);
    viewport.height = static_cast<float>(m_drawImageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    cmd.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent.width = m_drawImageExtent.width;
    scissor.extent.height = m_drawImageExtent.height;

    cmd.setScissor(0, 1, &scissor);
}

void Renderer::EndDepthPrePass(vk::CommandBuffer cmd) { vkCmdEndRendering(cmd); }

struct alignas(16) RendererData
{
    glm::vec2 screenSize;
    float     padding[2];
};

struct OcclusionObjectData
{
    BoundingBox boundingBox;
    n32         id;
};

using namespace Utils;

void Renderer::DoGPUOcclusionCulling(vk::CommandBuffer cmd, const std::vector<Utils::VisibleEntityInfo>& frustumCulledEntities, World& world,
                                     const Camera& cam)
{
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_computePipeline);

    std::vector<OcclusionObjectData> objectDataForGPU;
    objectDataForGPU.reserve(frustumCulledEntities.size());

    GetCurrentFrame().numObjectsDispatched = 0;

    for(const auto& entityInfo: frustumCulledEntities)
    {
        BoundingBox* bbComponent = world.GetComponent<BoundingBox>(entityInfo.id);

        if(bbComponent && bbComponent->valid)
        {
            OcclusionObjectData data;
            data.boundingBox = *bbComponent;
            data.id = entityInfo.id;

            objectDataForGPU.push_back(data);
        }
    }

    if(objectDataForGPU.empty()) { return; }

    GetCurrentFrame().numObjectsDispatched = objectDataForGPU.size();

    auto& objectDataBuffer = GetCurrentFrame().objectDataBuffer;
    auto& visibilityResultsBuffer = GetCurrentFrame().visibilityResults;
    auto& rendererDataBuffer = GetCurrentFrame().rendererDataBuffer;

    objectDataBuffer.reset();
    visibilityResultsBuffer.reset();
    rendererDataBuffer.reset();

    objectDataBuffer = std::make_unique<Buffer>();
    visibilityResultsBuffer = std::make_unique<Buffer>();
    rendererDataBuffer = std::make_unique<Buffer>();

    objectDataBuffer->Init(&m_logicalDevice, objectDataForGPU.size() * sizeof(OcclusionObjectData), 1, vk::BufferUsageFlagBits::eStorageBuffer,
                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO);

    visibilityResultsBuffer->Init(&m_logicalDevice, objectDataForGPU.size() * sizeof(VisiblityResultSet), 1,
                                  vk::BufferUsageFlagBits::eStorageBuffer,
                                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO);

    RendererData renderDataContent{{m_swapChain->GetExtent().width, m_swapChain->GetExtent().height}};
    rendererDataBuffer->Init(&m_logicalDevice, sizeof(RendererData), 1, vk::BufferUsageFlagBits::eUniformBuffer,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO);

    vk::MemoryBarrier2 memoryBarrier{};
    memoryBarrier.srcStageMask = vk::PipelineStageFlagBits2::eLateFragmentTests; // Or eEarlyFragmentTests if depth was written earlier
    memoryBarrier.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    memoryBarrier.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
    memoryBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead; // Compute shader reads depth

    vk::DependencyInfo dependencyInfo{};
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;
    cmd.pipelineBarrier2(&dependencyInfo);

    // Transition depth image layout for reading in compute shader
    Utils::ImageTransitionInfo preComputeReadTransition{
        cmd,
        vk::ImageLayout::eDepthStencilAttachmentOptimal, // Current layout after main rendering pass
        vk::ImageLayout::eShaderReadOnlyOptimal,         // Layout for compute shader to read
        &m_logicalDevice,
        m_depthImage.image,
        vk::ImageAspectFlagBits::eDepth,
    };
    Utils::TransitionImageLayout(preComputeReadTransition);

    // Map and Write data to GPU buffers
    objectDataBuffer->Map();
    objectDataBuffer->WriteToBuffer((void*)objectDataForGPU.data());
    objectDataBuffer->UnMap();

    rendererDataBuffer->Map();
    rendererDataBuffer->WriteToBuffer((void*)&renderDataContent);
    rendererDataBuffer->UnMap();

    // --- Descriptor Set Configuration (largely the same) ---
    vk::DescriptorImageInfo  depthInfo = {m_depthImageSampler, m_depthImage.imageView, vk::ImageLayout::eShaderReadOnlyOptimal};
    vk::DescriptorBufferInfo boundingBoxGpuInfo = objectDataBuffer->DescriptorInfo();      // Renamed to avoid conflict
    vk::DescriptorBufferInfo visiblityGpuInfo = visibilityResultsBuffer->DescriptorInfo(); // Renamed
    vk::DescriptorBufferInfo projectionGpuInfo = cam.GetCombinedDataBufferHandle(m_currentFrameIndex).DescriptorInfo();
    vk::DescriptorBufferInfo rendererDataGpuInfo = rendererDataBuffer->DescriptorInfo(); // Renamed

    vk::DescriptorSet& computeSet = GetCurrentFrame().computeSet;

    DescriptorWriter writer(*m_computeLayout, m_computePool.get());
    writer.WriteImage(0, &depthInfo)
        .WriteBuffer(1, &boundingBoxGpuInfo)
        .WriteBuffer(2, &visiblityGpuInfo)
        .WriteBuffer(3, &projectionGpuInfo)
        .WriteBuffer(4, &rendererDataGpuInfo);

    if(computeSet == VK_NULL_HANDLE) { writer.Build(computeSet); }
    else
    {
        writer.Overwrite(computeSet); // Or Update if your DescriptorWriter has that
    }

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_computePipelineLayout, 0, 1, &computeSet, 0, nullptr);

    uint32_t groupCountX = (static_cast<uint32_t>(objectDataForGPU.size()) + 63) / 64;
    if(groupCountX > 0)
    { // Only dispatch if there are objects
        vkCmdDispatch(cmd, groupCountX, 1, 1);
    }

    WaitForCompute(cmd);

    Utils::ImageTransitionInfo postComputeDepthTransition{cmd,
                                                          vk::ImageLayout::eShaderReadOnlyOptimal, // Current layout after compute shader read
                                                          vk::ImageLayout::eDepthStencilAttachmentOptimal, // Original layout for rendering
                                                          &m_logicalDevice,
                                                          m_depthImage.image,
                                                          vk::ImageAspectFlagBits::eDepth};
    Utils::TransitionImageLayout(postComputeDepthTransition);
}

void Renderer::WaitForCompute(vk::CommandBuffer cmd)
{
    vk::MemoryBarrier2 visibilityBarrier{};
    visibilityBarrier.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader;
    visibilityBarrier.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite;
    visibilityBarrier.dstStageMask = vk::PipelineStageFlagBits2::eHost;
    visibilityBarrier.dstAccessMask = vk::AccessFlagBits2::eHostRead;

    vk::DependencyInfo visibilityDependencyInfo{};
    visibilityDependencyInfo.memoryBarrierCount = 1;
    visibilityDependencyInfo.pMemoryBarriers = &visibilityBarrier;

    cmd.pipelineBarrier2(&visibilityDependencyInfo);
}

} // namespace Humongous
