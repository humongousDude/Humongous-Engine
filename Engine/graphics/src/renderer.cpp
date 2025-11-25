#include "renderer.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "extra.hpp"
#include "logger.hpp"
#include "ui/ui.hpp"

#include <array>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

/***
 * For image transitions, I've taken the approach of "Transition when needed", IE: Each pass has the responsibility of ensuring the images it uses
 * are in the correct layout. part of this is making sure passes do not have "post transitions", the "post" stage of a pass should be cleaning up
 * it's own resources, not preparing them for the next pass. Eventually, I plan to implement a frame graph to remove this headache entirely.
 */

namespace Humongous
{

Renderer::Renderer(Window& window, const ILogicalDevice& logicalDevice, const PhysicalDevice& physicalDevice, ResourceManager& resourceManager,
                   const IAssetManager& assetManager)
    : m_window{window}, m_logicalDevice{logicalDevice}, m_assetManager{assetManager}, m_resourceManager{resourceManager},
      m_physicalDevice{physicalDevice}

{

    CreateCommandPool();
    CreateCommandBuffers();
    CreateComputePipeline();
    CreateSyncStructures();
    CreateLightingPipeline();

    m_sceneExtent = UI::GetViewportSizePixels();
    RecreateSwapChain();
}

Renderer::~Renderer()
{
    HGINFO("Destroying renderer...");
    if(m_commandPool) { m_logicalDevice.GetVkDevice().destroyCommandPool(m_commandPool, nullptr); }

    for(Frame& frame: m_frames)
    {
        m_logicalDevice.GetVkDevice().destroySemaphore(frame.imageAvailableSemaphore, nullptr);
        m_logicalDevice.GetVkDevice().destroyFence(frame.inFlightFence, nullptr);

        frame.gbuffer.albedo.reset();
        frame.gbuffer.normalRough.reset();
        frame.gbuffer.materialParam.reset();
        frame.gbuffer.depth.reset();

        frame.visiblityResultBuffer.reset();
        frame.debugBuffer.reset();
        frame.objectDataBuffer.reset();
        frame.rendererDataBuffer.reset();

        frame.sceneImage.reset();
        frame.hiZImage.reset();
        for(auto& mip: frame.hiZMips)
        {
            m_logicalDevice.DestroyImageView(mip.sampledView);
            m_logicalDevice.DestroyImageView(mip.storageView);
        }
    }

    if(m_depthImageSampler != VK_NULL_HANDLE) { m_logicalDevice.DestroySampler(m_depthImageSampler); }

    m_occlusionPipeline.reset();
    m_logicalDevice.DestroyPipelineLayout(m_occlusionPipelineLayout);

    m_mipPipeline.reset();
    m_logicalDevice.DestroyPipelineLayout(m_mipPipelineLayout);

    m_lightingPipeline.reset();
    m_logicalDevice.DestroyPipelineLayout(m_lightingPipelineLayout);

    m_lightingDescriptorLayout.reset();
    m_lightingPool.reset();

    m_occlusionDescriptorLayout.reset();
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
        m_swapChain = std::make_unique<SwapChain>(m_window, m_physicalDevice, m_logicalDevice, oldSwapChain);
    }
    m_windowExtent = m_swapChain->GetExtent();

    RecreateViewport();

    m_logicalDevice.GetVkDevice().waitIdle();

    // We don't need to reset the frame index, since our resources are independent of the swapchain's image index
    // m_currentFrameIndex = 0;
    m_currentImageIndex = 0;

    m_window.ResetWindowResizedFlag();
    HGINFO("Recreated swap chain");
}

void Renderer::RecreateViewport()
{
    m_logicalDevice.GetVkDevice().waitIdle();
    CreateDrawImage();
    CreateGBuffer();

    for(u32 i = 0; i < static_cast<u32>(Globals::Limits::MaxFramesInFlight); ++i) { UI::RecreateViewportResources(*m_frames[i].sceneImage, i); }
}

void Renderer::CreateDrawImage()
{
    HGINFO("Creating draw image and view...");

    vk::ImageUsageFlags drawImageUsages{};
    drawImageUsages |= vk::ImageUsageFlagBits::eTransferSrc;
    drawImageUsages |= vk::ImageUsageFlagBits::eTransferDst;
    drawImageUsages |= vk::ImageUsageFlagBits::eStorage;
    drawImageUsages |= vk::ImageUsageFlagBits::eColorAttachment;
    drawImageUsages |= vk::ImageUsageFlagBits::eSampled;

    Image::ImageCreateInfo imgCI{};
    imgCI.layerCount = 1;
    imgCI.imageViewType = vk::ImageViewType::e2D;
    imgCI.tiling = vk::ImageTiling::eOptimal;
    imgCI.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imgCI.aspectFlags = vk::ImageAspectFlagBits::eColor;
    imgCI.mipLevels = 1;
    imgCI.usage = drawImageUsages;
    imgCI.layerCount = 1;
    imgCI.format = vk::Format::eR8G8B8A8Unorm;

    Image::SamplerCreateInfo samCI{};
    samCI.mipMode = vk::SamplerMipmapMode::eNearest;
    samCI.magFilter = vk::Filter::eLinear;
    samCI.minFilter = vk::Filter::eLinear;
    samCI.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samCI.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samCI.addressModeW = vk::SamplerAddressMode::eClampToEdge;

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();
    for(auto& frame: m_frames)
    {
        if(frame.sceneImage != nullptr && frame.sceneImage->IsValid()) { frame.sceneImage.reset(); }
        if(frame.windowImage != nullptr && frame.windowImage->IsValid()) { frame.windowImage.reset(); }

        imgCI.width = m_sceneExtent.width;
        imgCI.height = m_sceneExtent.height;
        imgCI.createWithSampler = true;
        imgCI.samplerInfo = &samCI;
        frame.sceneImage = std::make_unique<Image>(m_logicalDevice, imgCI);

        imgCI.width = m_windowExtent.width;
        imgCI.height = m_windowExtent.height;
        imgCI.createWithSampler = false;
        frame.windowImage = std::make_unique<Image>(m_logicalDevice, imgCI);

        frame.sceneImage->TransitionLayout(cmd, vk::ImageLayout::eGeneral);
        frame.windowImage->TransitionLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    }

    cmd.end();
    auto dep = m_logicalDevice.GetWorkScheduler().AddWork(cmd, m_logicalDevice.GetGraphicsQueue());
    GetCurrentFrame().workPackets.push_back(dep);

    HGINFO("Created draw image and view");
}

void Renderer::CreateGBuffer()
{
    HGINFO("Initializing G-Buffer...");

    vk::ImageUsageFlags colorUsages = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

    vk::SamplerReductionModeCreateInfo reductionInfo{};
    reductionInfo.reductionMode = vk::SamplerReductionMode::eMin;

    Image::SamplerCreateInfo samCI{};
    samCI.mipMode = vk::SamplerMipmapMode::eNearest;
    samCI.magFilter = vk::Filter::eLinear;
    samCI.minFilter = vk::Filter::eLinear;
    samCI.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samCI.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samCI.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samCI.pNext = &reductionInfo;

    Image::ImageCreateInfo imgCI{};
    imgCI.layerCount = 1;
    imgCI.imageViewType = vk::ImageViewType::e2D;
    imgCI.tiling = vk::ImageTiling::eOptimal;
    imgCI.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imgCI.aspectFlags = vk::ImageAspectFlagBits::eColor;
    imgCI.width = m_sceneExtent.width;
    imgCI.height = m_sceneExtent.height;
    imgCI.mipLevels = 1;
    imgCI.usage = colorUsages;
    imgCI.format = vk::Format::eR8G8B8A8Unorm;
    imgCI.createWithSampler = true;
    imgCI.samplerInfo = &samCI;

    if(!m_lightingPool)
    {
        DescriptorPool::Builder builder{m_logicalDevice};
        builder.AddPoolSize(vk::DescriptorType::eCombinedImageSampler, 45);
        builder.AddPoolSize(vk::DescriptorType::eStorageImage, 45);
        builder.SetMaxSets(50);
        m_lightingPool = builder.Build();
    }
    else
    {
        m_lightingPool->ResetPool();
    }

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();
    u32  mipLevels = floor(log2(std::max(imgCI.width, imgCI.height))) + 1;

    for(u32 i = 0; i < static_cast<u32>(Globals::Limits::MaxFramesInFlight); i++)
    {
        if(m_frames[i].gbuffer.albedo && m_frames[i].gbuffer.albedo->IsValid()) { m_frames[i].gbuffer.albedo.reset(); }
        if(m_frames[i].gbuffer.normalRough && m_frames[i].gbuffer.normalRough->IsValid()) { m_frames[i].gbuffer.normalRough.reset(); }
        if(m_frames[i].gbuffer.materialParam && m_frames[i].gbuffer.materialParam->IsValid()) { m_frames[i].gbuffer.materialParam.reset(); }
        if(m_frames[i].gbuffer.depth && m_frames[i].gbuffer.depth->IsValid()) { m_frames[i].gbuffer.depth.reset(); }
        if(m_frames[i].hiZImage && m_frames[i].hiZImage->IsValid()) { m_frames[i].hiZImage.reset(); }

        imgCI.usage = colorUsages;
        imgCI.format = vk::Format::eR8G8B8A8Unorm;
        imgCI.aspectFlags = vk::ImageAspectFlagBits::eColor;
        imgCI.mipLevels = 1;

        m_frames[i].gbuffer.albedo = std::make_unique<Image>(m_logicalDevice, imgCI);
        m_frames[i].gbuffer.albedo->TransitionLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);

        m_frames[i].gbuffer.normalRough = std::make_unique<Image>(m_logicalDevice, imgCI);
        m_frames[i].gbuffer.normalRough->TransitionLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);

        m_frames[i].gbuffer.materialParam = std::make_unique<Image>(m_logicalDevice, imgCI);
        m_frames[i].gbuffer.materialParam->TransitionLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);

        imgCI.aspectFlags = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        imgCI.format = vk::Format::eD32SfloatS8Uint;

        vk::ImageUsageFlags depthImageUsages{};
        depthImageUsages |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
        depthImageUsages |= vk::ImageUsageFlagBits::eSampled;
        depthImageUsages |= vk::ImageUsageFlagBits::eTransferSrc;
        imgCI.usage = depthImageUsages;

        m_frames[i].gbuffer.depth = std::make_unique<Image>(m_logicalDevice, imgCI);
        m_frames[i].gbuffer.depth->TransitionLayout(cmd, vk::ImageLayout::eDepthStencilAttachmentOptimal);

        imgCI.format = vk::Format::eR32Sfloat;
        imgCI.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
        imgCI.aspectFlags = vk::ImageAspectFlagBits::eColor;
        imgCI.mipLevels = mipLevels;

        m_frames[i].hiZImage = std::make_unique<Image>(m_logicalDevice, imgCI);
        m_frames[i].hiZImage->TransitionLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);

        for(auto& mip: m_frames[i].hiZMips)
        {
            if(mip.sampledView != VK_NULL_HANDLE) { m_logicalDevice.DestroyImageView(mip.sampledView); }
            if(mip.storageView != VK_NULL_HANDLE) { m_logicalDevice.DestroyImageView(mip.storageView); }
        }

        m_frames[i].hiZMips.resize(imgCI.mipLevels);
        for(u32 level = 0; level < imgCI.mipLevels; ++level)
        {
            m_frames[i].hiZImage->TransitionLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal, level, 1, 0, 1);

            vk::ImageViewCreateInfo viewInfo{};
            viewInfo.image = m_frames[i].hiZImage->GetImage();
            viewInfo.format = vk::Format::eR32Sfloat;
            viewInfo.viewType = vk::ImageViewType::e2D;
            viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseMipLevel = level;

            if(m_logicalDevice.CreateImageView(viewInfo, &m_frames[i].hiZMips[level].sampledView) != vk::Result::eSuccess)
            {
                HGFATAL("Failed to create sampled depthMip");
            }

            if(m_logicalDevice.CreateImageView(viewInfo, &m_frames[i].hiZMips[level].storageView) != vk::Result::eSuccess)
            {
                HGFATAL("Failed to create storage depthMip");
            }

            if(m_frames[i].hiZMips[level].set == VK_NULL_HANDLE)
            {
                m_computePool->AllocateDescriptor(m_mipDescriptorLayout->GetDescriptorSetLayout(), m_frames[i].hiZMips[level].set);
            }

            m_frames[i].hiZMips[level].layout = vk::ImageLayout::eShaderReadOnlyOptimal;
        }

        m_lightingPool->AllocateDescriptor(m_lightingDescriptorLayout->GetDescriptorSetLayout(), m_frames[i].gbuffer.imageSet);
    }

    cmd.end();
    auto dep = m_logicalDevice.GetWorkScheduler().AddWork(cmd, m_logicalDevice.GetGraphicsQueue());
    GetCurrentFrame().workPackets.push_back(dep);

    HGINFO("Initialized G-Buffer (resized to %d x %d)", m_sceneExtent.width, m_sceneExtent.height);
}

void Renderer::CreateLightingPipeline()
{
    HGINFO("Creating lighting layout...");
    DescriptorSetLayout::Builder builder{m_logicalDevice};
    builder.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);
    builder.AddBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);
    builder.AddBinding(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);
    builder.AddBinding(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);
    builder.AddBinding(4, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
    m_lightingDescriptorLayout = builder.Build();

    DescriptorSetLayout::Builder scene{m_logicalDevice};
    scene.AddBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute, 1);
    auto layout = scene.Build();

    DescriptorSetLayout::Builder cam{m_logicalDevice};
    cam.AddBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute, 1);
    auto camlayout = cam.Build();

    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
    descriptorSetLayouts.push_back(camlayout->GetDescriptorSetLayout());
    descriptorSetLayouts.push_back(layout->GetDescriptorSetLayout());
    descriptorSetLayouts.push_back(m_resourceManager.GetSkyboxCompDescriptorLayout());
    descriptorSetLayouts.push_back(m_lightingDescriptorLayout->GetDescriptorSetLayout());

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<u32>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if(m_logicalDevice.CreatePipelineLayout(pipelineLayoutInfo, &m_lightingPipelineLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create lighting layout");
        return;
    }

    HGINFO("Created lighting layout");

    HGINFO("Creating pipeline...");

    ComputePipeline::ComputePipelineCreateInfo configInfo{.logicalDevice = m_logicalDevice, .pipelineLayout = m_lightingPipelineLayout};
    configInfo.shaderFile = m_assetManager.GetAsset(AssetManager::AssetType::SHADER, "lighting.comp");

    m_lightingPipeline = std::make_unique<ComputePipeline>(configInfo);

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
        return;
    }

    HGINFO("Created command pool");
}

void Renderer::CreateCommandBuffers()
{
    HGINFO("Allocating command buffers...");

    m_frames.resize(static_cast<u32>(Globals::Limits::MaxFramesInFlight));

    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = static_cast<u32>(m_frames.size());

    for(Frame& frame: m_frames)
    {
        if(m_logicalDevice.GetVkDevice().allocateCommandBuffers(&allocInfo, &frame.commandBuffer) != vk::Result::eSuccess)
        {
            HGERROR("Failed to allocate command buffers");
        }
    }

    HGINFO("Allocated command buffers");
}

void Renderer::CreateSyncStructures()
{
    HGINFO("Initializing synchronization structures...");

    // one fence to control when the gpu has finished rendering the frame
    // and 2 semaphores to synchronize rendering with swapchain
    vk::FenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    vk::SemaphoreCreateInfo semaphoreCreateInfo{};

    for(u32 i = 0; i < static_cast<u32>(Globals::Limits::MaxFramesInFlight); i++)
    {
        // wait just in case
        if(m_frames[i].inFlightFence != VK_NULL_HANDLE &&
           m_logicalDevice.GetVkDevice().waitForFences(1, &m_frames[i].inFlightFence, VK_TRUE, std::numeric_limits<u64>::max()) !=
               vk::Result::eSuccess)
        {
            HGERROR("Failed to wait for fence for frame %i", i);
            continue;
        }

        if(m_frames[i].inFlightFence != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroyFence(m_frames[i].inFlightFence, nullptr); }

        if(m_logicalDevice.GetVkDevice().createFence(&fenceCreateInfo, nullptr, &m_frames[i].inFlightFence) != vk::Result::eSuccess)
        {
            HGERROR("Failed to create fence");
        }

        // if(m_frames[i].imageAvailableSemaphore != VK_NULL_HANDLE)
        // {
        //     m_logicalDevice.GetVkDevice().destroySemaphore(m_frames[i].imageAvailableSemaphore, nullptr);
        // }

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
    builder.AddPoolSize(vk::DescriptorType::eCombinedImageSampler, 100);
    builder.AddPoolSize(vk::DescriptorType::eStorageBuffer, 100);
    builder.AddPoolSize(vk::DescriptorType::eUniformBuffer, 100);
    builder.AddPoolSize(vk::DescriptorType::eStorageImage, 100);
    builder.SetMaxSets(1000);
    m_computePool = builder.Build();

    DescriptorSetLayout::Builder builder2{m_logicalDevice};
    builder2.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(3, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(4, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(5, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute);
    m_occlusionDescriptorLayout = builder2.Build();

    DescriptorSetLayout::Builder builder3{m_logicalDevice};
    builder3.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    builder3.AddBinding(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute);
    m_mipDescriptorLayout = builder3.Build();

    for(auto& frame: m_frames) { m_computePool->AllocateDescriptor(m_occlusionDescriptorLayout->GetDescriptorSetLayout(), frame.occlusionSet); }
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

    // Occlusion
    {
        auto layout = m_occlusionDescriptorLayout->GetDescriptorSetLayout();

        vk::PushConstantRange range{vk::ShaderStageFlagBits::eCompute, 0, sizeof(u32)};

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &layout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &range;

        if(m_logicalDevice.GetVkDevice().createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_occlusionPipelineLayout) != vk::Result::eSuccess)
        {
            HGERROR("Failed to create pipeline layout");
        }

        ComputePipeline::ComputePipelineCreateInfo configInfo{.logicalDevice = m_logicalDevice, .pipelineLayout = m_occlusionPipelineLayout};
        configInfo.shaderFile = m_assetManager.GetAsset(AssetManager::AssetType::SHADER, "occlusion.comp");

        m_occlusionPipeline = std::make_unique<ComputePipeline>(configInfo);
    }
    // Mipmap
    {
        auto layout = m_mipDescriptorLayout->GetDescriptorSetLayout();

        vk::PushConstantRange range{vk::ShaderStageFlagBits::eCompute, 0, sizeof(u32)};

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &layout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &range;

        if(m_logicalDevice.GetVkDevice().createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_mipPipelineLayout) != vk::Result::eSuccess)
        {
            HGERROR("Failed to create pipeline layout");
        }

        ComputePipeline::ComputePipelineCreateInfo configInfo{.logicalDevice = m_logicalDevice, .pipelineLayout = m_mipPipelineLayout};
        configInfo.shaderFile = m_assetManager.GetAsset(AssetManager::AssetType::SHADER, "mip.comp");

        m_mipPipeline = std::make_unique<ComputePipeline>(configInfo);
    }
    HGINFO("Created compute pipeline");
}

void Renderer::ReadyPerFrameData(std::vector<Utils::VisibleEntityInfo>& visibleEntities)
{

    auto& visibilityResultsBuffer = GetCurrentFrame().visiblityResultBuffer;
    u32   numObjectsCulledLastFrame = GetCurrentFrame().numObjectsDispatched;

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

    for(u32 i = 0; i < numObjectsCulledLastFrame; ++i)
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

vk::CommandBuffer Renderer::BeginFrame(std::vector<Utils::VisibleEntityInfo>& visibleEntities)
{
    vk::Result result = m_logicalDevice.GetVkDevice().waitForFences(1, &GetCurrentFrame().inFlightFence, vk::True, std::numeric_limits<u64>::max());
    if(result != vk::Result::eSuccess)
    {
        HGERROR("Failed to wait for fences: %s", vk::to_string(result).c_str());
        return VK_NULL_HANDLE;
    }

    result = m_logicalDevice.GetVkDevice().resetFences(1, &GetCurrentFrame().inFlightFence);
    if(result != vk::Result::eSuccess) { HGERROR("Failed to reset fences: %s", vk::to_string(result).c_str()); }

    result = m_swapChain->AcquireNextImage(GetCurrentFrame().imageAvailableSemaphore, m_currentImageIndex);
    if(result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_window.WasWindowResized())
    {
        RecreateSwapChain();
        return VK_NULL_HANDLE;
    }

    if(result != vk::Result::eSuccess) { HGERROR("failed to acquire swap chain image!"); }

    vk::Extent2D desiredExtent = UI::GetViewportSizePixels();
    if(desiredExtent != m_sceneExtent)
    {
        m_sceneExtent = desiredExtent;
        RecreateViewport();
        return VK_NULL_HANDLE;
    }

    ReadyPerFrameData(visibleEntities);

    auto cmd = GetCurrentFrame().commandBuffer;
    cmd.reset();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

    if(cmd.begin(&beginInfo) != vk::Result::eSuccess)
    {
        HGERROR("Failed to begin recording command buffer");
        return VK_NULL_HANDLE;
    }

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = m_sceneExtent.width;
    viewport.height = m_sceneExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    cmd.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent.width = m_sceneExtent.width;
    scissor.extent.height = m_sceneExtent.height;

    cmd.setScissor(0, 1, &scissor);

    m_logicalDevice.GetWorkScheduler().CollectGarbage();
    GetCurrentFrame().started = true;

    return cmd;
}

void Renderer::EndFrame()
{
    auto cmd = GetCurrentFrame().commandBuffer;
    if(!GetCurrentFrame().started)
    {
        HGINFO("Cannot end a frame that has not been begun!");

        m_logicalDevice.GetWorkScheduler().Flush(GetCurrentFrame().inFlightFence);
        m_currentFrameIndex = (m_currentFrameIndex + 1) % static_cast<u32>(Globals::Limits::MaxFramesInFlight);
        return;
    }

    GetCurrentFrame().windowImage->TransitionLayout(cmd, vk::ImageLayout::eTransferSrcOptimal);

    Utils::ImageTransitionInfo swapInfo{.cmd = cmd, .logicalDevice = m_logicalDevice, .image = m_swapChain->GetImages()[m_currentImageIndex]};
    swapInfo.oldLayout = vk::ImageLayout::eUndefined;
    swapInfo.newLayout = vk::ImageLayout::eTransferDstOptimal;

    Utils::TransitionImageLayout(swapInfo);

    Image::CopyToImage(m_logicalDevice, cmd, GetCurrentFrame().windowImage->GetImage(), m_swapChain->GetImages()[m_currentImageIndex],
                       m_windowExtent, m_swapChain->GetExtent());

    Utils::ImageTransitionInfo presentTransitionInfo{.cmd = cmd,
                                                     .logicalDevice = m_logicalDevice,
                                                     .image = m_swapChain->GetImages()[m_currentImageIndex]};
    presentTransitionInfo.image = m_swapChain->GetImages()[m_currentImageIndex];
    presentTransitionInfo.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    presentTransitionInfo.newLayout = vk::ImageLayout::ePresentSrcKHR;
    presentTransitionInfo.cmd = cmd;

    Utils::TransitionImageLayout(presentTransitionInfo);

    GetCurrentFrame().windowImage->TransitionLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);

    cmd.end();
    m_logicalDevice.GetWorkScheduler().AddWork(cmd, m_logicalDevice.GetGraphicsQueue(), GetCurrentFrame().workPackets);

    m_logicalDevice.GetWorkScheduler().Flush(GetCurrentFrame().inFlightFence, GetCurrentFrame().imageAvailableSemaphore,
                                             m_swapChain->GetRenderFinishedSemaphoreAtIndex(m_currentImageIndex));

    m_swapChain->Present(m_swapChain->GetRenderFinishedSemaphoreAtIndex(m_currentImageIndex), m_currentImageIndex);

    GetCurrentFrame().started = false;
    m_currentFrameIndex = (m_currentFrameIndex + 1) % static_cast<u32>(Globals::Limits::MaxFramesInFlight);
}

void Renderer::BeginGeometryPass(vk::CommandBuffer cmd)
{
    auto& currentFrame = GetCurrentFrame();

    currentFrame.gbuffer.albedo->TransitionLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    currentFrame.gbuffer.normalRough->TransitionLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    currentFrame.gbuffer.materialParam->TransitionLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    currentFrame.gbuffer.depth->TransitionLayout(cmd, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {0.5f, 0.5f, 0.5f, 0.5f};
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(0.0f, 0);

    vk::RenderingAttachmentInfo attach{};
    attach.sType = vk::StructureType::eRenderingAttachmentInfo;
    attach.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attach.loadOp = vk::AttachmentLoadOp::eClear;
    attach.storeOp = vk::AttachmentStoreOp::eStore;
    attach.clearValue = clearValues[0];

    std::array<vk::RenderingAttachmentInfo, 3> colorAttachments{attach, attach, attach};
    colorAttachments[0].imageView = currentFrame.gbuffer.albedo->GetImageView();
    colorAttachments[1].imageView = currentFrame.gbuffer.normalRough->GetImageView();
    colorAttachments[2].imageView = currentFrame.gbuffer.materialParam->GetImageView();

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    depthAttachment.imageView = currentFrame.gbuffer.depth->GetImageView();
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue = clearValues[1];

    vk::RenderingAttachmentInfo stencilAttachment{};
    stencilAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    stencilAttachment.imageView = currentFrame.gbuffer.depth->GetImageView();
    stencilAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    stencilAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    stencilAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    stencilAttachment.clearValue = clearValues[1];

    vk::RenderingInfo renderingInfo{};
    renderingInfo.sType = vk::StructureType::eRenderingInfo;
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D(0, 0), m_sceneExtent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = &stencilAttachment;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;

    cmd.beginRendering(&renderingInfo);
}

void Renderer::EndGeometryPass(vk::CommandBuffer cmd) { cmd.endRendering(); }

void Renderer::DoLightingPass(vk::CommandBuffer cmd, vk::DescriptorSet camSet, vk::DescriptorSet sceneSet, vk::DescriptorSet skyboxSet)
{
    auto& currentFrame = GetCurrentFrame();

    currentFrame.gbuffer.albedo->TransitionLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    currentFrame.gbuffer.normalRough->TransitionLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    currentFrame.gbuffer.materialParam->TransitionLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    currentFrame.gbuffer.depth->TransitionLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);

    currentFrame.sceneImage->TransitionLayout(cmd, vk::ImageLayout::eGeneral);

    auto albedoInfo = currentFrame.gbuffer.albedo->GetDescriptorInfo();
    auto normalInfo = currentFrame.gbuffer.normalRough->GetDescriptorInfo();
    auto matInfo = currentFrame.gbuffer.materialParam->GetDescriptorInfo();
    auto depthInfo = currentFrame.gbuffer.depth->GetDescriptorInfo();
    auto drawInfo = currentFrame.sceneImage->GetDescriptorInfo();

    DescriptorWriter writer(*m_lightingDescriptorLayout, m_lightingPool.get());
    writer.WriteImage(0, &albedoInfo)
        .WriteImage(1, &normalInfo)
        .WriteImage(2, &matInfo)
        .WriteImage(3, &depthInfo)
        .WriteImage(4, &drawInfo)
        .Overwrite(currentFrame.gbuffer.imageSet);

    m_lightingPipeline->BindPipeline(cmd);

    std::vector<vk::DescriptorSet> sets = {camSet, sceneSet, skyboxSet, currentFrame.gbuffer.imageSet};
    m_logicalDevice.RecordBindDescriptorSets(cmd, m_lightingPipelineLayout, vk::PipelineBindPoint::eCompute, 0, sets);

    u32 localSize = 8;

    u32 countX = std::ceil(m_sceneExtent.width / localSize);
    u32 countY = std::ceil(m_sceneExtent.height / localSize);

    m_logicalDevice.RecordComputeDispatch(cmd, countX, countY, 1);

    WaitForCompute(cmd);
}

void Renderer::BeginSkyboxPass(vk::CommandBuffer cmd)
{
    GetCurrentFrame().sceneImage->TransitionLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);

    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(0.0f, 0);

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    colorAttachment.imageView = GetCurrentFrame().sceneImage->GetImageView();
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = clearValues[0];

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    depthAttachment.imageView = GetCurrentFrame().gbuffer.depth->GetImageView();
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.clearValue = clearValues[1];

    vk::RenderingInfo renderingInfo{};
    renderingInfo.sType = vk::StructureType::eRenderingInfo;
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D(0, 0), m_sceneExtent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = &depthAttachment;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;

    cmd.beginRendering(&renderingInfo);
}

void Renderer::EndSkyboxPass(vk::CommandBuffer cmd) { cmd.endRendering(); }

void Renderer::BeginUIPass(vk::CommandBuffer cmd)
{
    GetCurrentFrame().sceneImage->TransitionLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = m_windowExtent.width;
    viewport.height = m_windowExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent.width = m_windowExtent.width;
    scissor.extent.height = m_windowExtent.height;
    cmd.setScissor(0, 1, &scissor);

    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {0.3f, 0.3f, 0.3f, 0.3f};

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    colorAttachment.imageView = GetCurrentFrame().windowImage->GetImageView();
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = clearValues[0];

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D(0, 0), m_windowExtent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;

    cmd.beginRendering(&renderingInfo);
}

void Renderer::EndUIPass(vk::CommandBuffer cmd) { cmd.endRendering(); }

void Renderer::BeginDepthPrePass(vk::CommandBuffer cmd)
{
    GetCurrentFrame().gbuffer.depth->TransitionLayout(cmd, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::ClearValue clearValue{};
    clearValue.depthStencil = vk::ClearDepthStencilValue{0.0f, 0};

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    depthAttachment.imageView = GetCurrentFrame().gbuffer.depth->GetImageView();
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue = clearValue;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D(0, 0), m_sceneExtent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 0;
    renderingInfo.pColorAttachments = nullptr;
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;

    cmd.beginRendering(&renderingInfo);
}

void Renderer::EndDepthPrePass(vk::CommandBuffer cmd) { cmd.endRendering(); }

struct alignas(16) RendererData
{
    Eigen::Vector2f screenSize;
    float           padding[2];
};

struct OcclusionObjectData
{
    BoundingBox boundingBox;
    u32         id;
};

using namespace Utils;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"

void Renderer::DoOcclusionCulling(vk::CommandBuffer cmd, const std::vector<Utils::VisibleEntityInfo>& frustumCulledEntities, World& world,
                                  const Camera& cam)
{
    HGWARN("Occlusion culling is currently disabled");
    return;
}

#pragma GCC diagnostic pop

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
