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

        frame.visiblityResultBuffer.reset();
        frame.debugBuffer.reset();
        frame.objectDataBuffer.reset();
        frame.rendererDataBuffer.reset();
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

    m_logicalDevice.GetVkDevice().waitIdle();

    m_screenImageExtent = m_swapChain->GetExtent();
    HGINFO("SCREEN IS %i by %i", m_screenImageExtent.width, m_screenImageExtent.height);
    m_drawImage.imageExtent = vk::Extent3D(m_screenImageExtent.width, m_screenImageExtent.height, 0.0);

    InitDrawImage();
    InitDepthImage();
    InitGBuffer();
}

void Renderer::InitDrawImage()
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
    imgCI.width = m_screenImageExtent.width;
    imgCI.height = m_screenImageExtent.height;
    imgCI.mipLevels = 1;
    imgCI.usage = drawImageUsages;
    imgCI.layerCount = 1;
    imgCI.format = m_drawImage.imageFormat == vk::Format::eUndefined ? vk::Format::eR16G16B16A16Sfloat : m_drawImage.imageFormat;
    imgCI.imagePool = VK_NULL_HANDLE;
    imgCI.samples = vk::SampleCountFlagBits::e1;

    Utils::CreateAllocatedImage(imgCI);

    auto                       cmd = m_logicalDevice.BeginSingleTimeCommands();
    Utils::ImageTransitionInfo transInfo{};
    transInfo.image = m_drawImage.image;
    transInfo.oldLayout = vk::ImageLayout::eUndefined;
    transInfo.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    transInfo.cmd = cmd;
    transInfo.imageAspect = vk::ImageAspectFlagBits::eColor;
    Utils::TransitionImageLayout(transInfo);

    m_drawImage.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;

    m_logicalDevice.EndSingleTimeCommands(cmd);

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
    imgCI.width = m_screenImageExtent.width;
    imgCI.height = m_screenImageExtent.height;
    imgCI.mipLevels = 1;
    imgCI.usage = depthImageUsages;
    imgCI.format = vk::Format::eD32Sfloat;
    imgCI.imagePool = VK_NULL_HANDLE;
    imgCI.samples = vk::SampleCountFlagBits::e1;

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();

    Utils::CreateAllocatedImage(imgCI);

    Utils::ImageTransitionInfo postComputeDepthTransition{cmd,
                                                          vk::ImageLayout::eUndefined,
                                                          vk::ImageLayout::eDepthAttachmentOptimal,
                                                          &m_logicalDevice,
                                                          m_depthImage.image,
                                                          vk::ImageAspectFlagBits::eDepth};

    Utils::TransitionImageLayout(postComputeDepthTransition);

    m_logicalDevice.EndSingleTimeCommands(cmd);

    m_depthImage.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;

    HGINFO("Created depth image and view");
}

void Renderer::InitGBuffer()
{
    HGINFO("Initializing G-Buffer (resize or first‐time)…");

    vk::ImageUsageFlags colorUsages = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

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
    imgCI.width = m_screenImageExtent.width;
    imgCI.height = m_screenImageExtent.height;
    imgCI.mipLevels = 1;
    imgCI.usage = colorUsages;
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
        imgCI.usage = colorUsages;
        imgCI.format = vk::Format::eR16G16B16A16Sfloat;
        imgCI.aspectFlags = vk::ImageAspectFlagBits::eColor;
        imgCI.initialLayout = vk::ImageLayout::eUndefined;
        Utils::CreateAllocatedImage(imgCI);

        {
            Utils::ImageTransitionInfo trans{};
            trans.cmd = cmd;
            trans.image = m_frames[i].gbuffer.albedo.image;
            trans.oldLayout = vk::ImageLayout::eUndefined;
            trans.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            trans.imageAspect = vk::ImageAspectFlagBits::eColor;
            Utils::TransitionImageLayout(trans);
            // Record that frame i’s albedo is now in COLOR_ATTACHMENT_OPTIMAL
            m_frames[i].gbuffer.albedo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        }

        imgCI.allocatedImage = &m_frames[i].gbuffer.normalRough;
        Utils::CreateAllocatedImage(imgCI);
        {
            Utils::ImageTransitionInfo trans{};
            trans.cmd = cmd;
            trans.image = m_frames[i].gbuffer.normalRough.image;
            trans.oldLayout = vk::ImageLayout::eUndefined;
            trans.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            trans.imageAspect = vk::ImageAspectFlagBits::eColor;
            Utils::TransitionImageLayout(trans);
            m_frames[i].gbuffer.normalRough.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        }

        imgCI.allocatedImage = &m_frames[i].gbuffer.materialParam;
        Utils::CreateAllocatedImage(imgCI);
        {
            Utils::ImageTransitionInfo trans{};
            trans.cmd = cmd;
            trans.image = m_frames[i].gbuffer.materialParam.image;
            trans.oldLayout = vk::ImageLayout::eUndefined;
            trans.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            trans.imageAspect = vk::ImageAspectFlagBits::eColor;
            Utils::TransitionImageLayout(trans);
            m_frames[i].gbuffer.materialParam.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        }

        imgCI.allocatedImage = &m_frames[i].gbuffer.position;
        Utils::CreateAllocatedImage(imgCI);
        {
            Utils::ImageTransitionInfo trans{};
            trans.cmd = cmd;
            trans.image = m_frames[i].gbuffer.position.image;
            trans.oldLayout = vk::ImageLayout::eUndefined;
            trans.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            trans.imageAspect = vk::ImageAspectFlagBits::eColor;
            Utils::TransitionImageLayout(trans);
            m_frames[i].gbuffer.position.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        }

        imgCI.allocatedImage = &m_frames[i].gbuffer.depth;
        imgCI.aspectFlags = vk::ImageAspectFlagBits::eDepth;
        imgCI.format = vk::Format::eD32Sfloat;
        imgCI.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
        imgCI.initialLayout = vk::ImageLayout::eUndefined;
        Utils::CreateAllocatedImage(imgCI);
        {
            Utils::ImageTransitionInfo trans{};
            trans.cmd = cmd;
            trans.image = m_frames[i].gbuffer.depth.image;
            trans.oldLayout = vk::ImageLayout::eUndefined;
            trans.newLayout = vk::ImageLayout::eDepthAttachmentOptimal;
            trans.imageAspect = vk::ImageAspectFlagBits::eDepth;
            Utils::TransitionImageLayout(trans);
            m_frames[i].gbuffer.depth.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
        }

        m_lightingPool->AllocateDescriptor(m_lightingLayout->GetDescriptorSetLayout(), m_frames[i].gbuffer.imageSet);
    }

    m_logicalDevice.EndSingleTimeCommands(cmd);

    HGINFO("Initialized G-Buffer (resized to %d x %d)", m_screenImageExtent.width, m_screenImageExtent.height);
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

    vk::PushConstantRange range{vk::ShaderStageFlagBits::eCompute, 0, sizeof(n32)};

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &range;

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

    auto&    visibilityResultsBuffer = GetCurrentFrame().visiblityResultBuffer;
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

void Renderer::PreGeometryPassTransitions(vk::CommandBuffer cmd)
{
    auto& currentFrame = GetCurrentFrame();

    // check for first frame, only need the first image because all images should be transitioned at the same time
    // if(currentFrame.gbuffer.albedo.imageLayout == vk::ImageLayout::eColorAttachmentOptimal &&
    //    currentFrame.gbuffer.normalRough.imageLayout == vk::ImageLayout::eColorAttachmentOptimal &&
    //    currentFrame.gbuffer.materialParam.imageLayout == vk::ImageLayout::eColorAttachmentOptimal &&
    //    currentFrame.gbuffer.position.imageLayout == vk::ImageLayout::eColorAttachmentOptimal &&
    //    currentFrame.gbuffer.depth.imageLayout == vk::ImageLayout::eDepthAttachmentOptimal)
    // {
    //     return;
    // }

    Utils::ImageTransitionInfo drawInfo{};
    drawInfo.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    drawInfo.imageAspect = vk::ImageAspectFlagBits::eColor;
    drawInfo.cmd = cmd;

    {
        drawInfo.image = currentFrame.gbuffer.albedo.image;
        drawInfo.oldLayout = currentFrame.gbuffer.albedo.imageLayout;
        currentFrame.gbuffer.albedo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        drawInfo.image = currentFrame.gbuffer.normalRough.image;
        drawInfo.oldLayout = currentFrame.gbuffer.normalRough.imageLayout;
        currentFrame.gbuffer.normalRough.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        drawInfo.image = currentFrame.gbuffer.materialParam.image;
        drawInfo.oldLayout = currentFrame.gbuffer.materialParam.imageLayout;
        currentFrame.gbuffer.materialParam.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        drawInfo.image = currentFrame.gbuffer.position.image;
        drawInfo.oldLayout = currentFrame.gbuffer.position.imageLayout;
        currentFrame.gbuffer.position.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        drawInfo.image = currentFrame.gbuffer.depth.image;
        drawInfo.oldLayout = currentFrame.gbuffer.depth.imageLayout;
        drawInfo.newLayout = vk::ImageLayout::eDepthAttachmentOptimal;
        drawInfo.imageAspect = vk::ImageAspectFlagBits::eDepth;
        currentFrame.gbuffer.depth.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
}

void Renderer::PostGeometryPassTransitions(vk::CommandBuffer cmd)
{
    auto& currentFrame = GetCurrentFrame();

    Utils::ImageTransitionInfo drawInfo{};
    drawInfo.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    drawInfo.cmd = cmd;
    drawInfo.imageAspect = vk::ImageAspectFlagBits::eColor;

    {
        drawInfo.image = currentFrame.gbuffer.albedo.image;
        drawInfo.oldLayout = currentFrame.gbuffer.albedo.imageLayout;
        currentFrame.gbuffer.albedo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        drawInfo.image = currentFrame.gbuffer.normalRough.image;
        drawInfo.oldLayout = currentFrame.gbuffer.normalRough.imageLayout;
        currentFrame.gbuffer.normalRough.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        drawInfo.image = currentFrame.gbuffer.materialParam.image;
        drawInfo.oldLayout = currentFrame.gbuffer.materialParam.imageLayout;
        currentFrame.gbuffer.materialParam.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        drawInfo.image = currentFrame.gbuffer.position.image;
        drawInfo.oldLayout = currentFrame.gbuffer.position.imageLayout;
        currentFrame.gbuffer.position.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
    {
        drawInfo.image = currentFrame.gbuffer.depth.image;
        drawInfo.oldLayout = currentFrame.gbuffer.depth.imageLayout;
        drawInfo.imageAspect = vk::ImageAspectFlagBits::eDepth;
        currentFrame.gbuffer.depth.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
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

    if(m_drawImage.imageLayout == vk::ImageLayout::eTransferSrcOptimal)
    {
        auto                       cmd = m_logicalDevice.BeginSingleTimeCommands();
        Utils::ImageTransitionInfo transInfo{};
        transInfo.image = m_drawImage.image;
        transInfo.oldLayout = m_drawImage.imageLayout;
        transInfo.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        transInfo.cmd = cmd;
        transInfo.imageAspect = vk::ImageAspectFlagBits::eColor;
        Utils::TransitionImageLayout(transInfo);
        m_drawImage.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        m_logicalDevice.EndSingleTimeCommands(cmd);
    }

    m_screenImageExtent = m_swapChain->GetExtent();

    m_drawImage.imageExtent = vk::Extent3D(m_screenImageExtent, 0.0);
    m_depthImage.imageExtent = vk::Extent3D(m_screenImageExtent, 0.0);

    auto cmd = GetCurrentFrame().commandBuffer;
    cmd.reset();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

    if(cmd.begin(&beginInfo) != vk::Result::eSuccess) { HGERROR("Failed to begin recording command buffer"); }

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_screenImageExtent.width);
    viewport.height = static_cast<float>(m_screenImageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    cmd.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent.width = m_screenImageExtent.width;
    scissor.extent.height = m_screenImageExtent.height;

    cmd.setScissor(0, 1, &scissor);

    return cmd;
}

void Renderer::EndFrame()
{
    auto cmd = GetCurrentFrame().commandBuffer;

    Utils::ImageTransitionInfo drawInfo{};
    drawInfo.image = m_drawImage.image;
    drawInfo.oldLayout = m_drawImage.imageLayout;
    drawInfo.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    drawInfo.cmd = cmd;
    Utils::TransitionImageLayout(drawInfo);

    m_drawImage.imageLayout = vk::ImageLayout::eTransferSrcOptimal;

    Utils::ImageTransitionInfo swapInfo{};
    swapInfo.image = m_swapChain->GetImages()[m_currentImageIndex];
    swapInfo.oldLayout = vk::ImageLayout::eUndefined;
    swapInfo.newLayout = vk::ImageLayout::eTransferDstOptimal;
    swapInfo.cmd = cmd;

    Utils::TransitionImageLayout(swapInfo);

    Utils::CopyImageToImage(cmd, m_drawImage.image, m_swapChain->GetImages()[m_currentImageIndex], m_screenImageExtent, m_swapChain->GetExtent());

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
    PreGeometryPassTransitions(cmd);

    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo attach{};
    attach.sType = vk::StructureType::eRenderingAttachmentInfo;
    attach.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attach.loadOp = vk::AttachmentLoadOp::eClear;
    attach.storeOp = vk::AttachmentStoreOp::eStore;
    attach.clearValue = clearValues[0];

    auto& currentFrame = GetCurrentFrame();

    std::array<vk::RenderingAttachmentInfo, 4> colorAttachments{attach, attach, attach, attach};
    colorAttachments[0].imageView = currentFrame.gbuffer.albedo.imageView;
    colorAttachments[1].imageView = currentFrame.gbuffer.normalRough.imageView;
    colorAttachments[2].imageView = currentFrame.gbuffer.materialParam.imageView;
    colorAttachments[3].imageView = currentFrame.gbuffer.position.imageView;

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    depthAttachment.imageView = currentFrame.gbuffer.depth.imageView;
    depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue = clearValues[1];

    vk::RenderingInfo renderingInfo{};
    renderingInfo.sType = vk::StructureType::eRenderingInfo;
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D(0, 0), m_screenImageExtent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    // renderingInfo.flags = 0;

    cmd.beginRendering(&renderingInfo);
}

void Renderer::EndGeometryPass(vk::CommandBuffer cmd)
{
    cmd.endRendering();
    PostGeometryPassTransitions(cmd);
}

void Renderer::DoLightingPass(vk::CommandBuffer cmd, vk::DescriptorSet camSet, vk::DescriptorSet sceneSet, vk::DescriptorSet skyboxSet)
{
    vk::ClearValue clearValue{};
    clearValue.color = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    colorAttachment.imageView = m_drawImage.imageView;
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = clearValue;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.sType = vk::StructureType::eRenderingInfo;
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D(0, 0), m_screenImageExtent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    // renderingInfo.flags = 0;

    cmd.beginRendering(&renderingInfo);

    auto albedoInfo = GetCurrentFrame().gbuffer.albedo.GetDescriptorInfo();
    auto normalInfo = GetCurrentFrame().gbuffer.normalRough.GetDescriptorInfo();
    auto matInfo = GetCurrentFrame().gbuffer.materialParam.GetDescriptorInfo();
    auto posInfo = GetCurrentFrame().gbuffer.position.GetDescriptorInfo();
    auto depthInfo = GetCurrentFrame().gbuffer.depth.GetDescriptorInfo();

    DescriptorWriter writer(*m_lightingLayout, m_lightingPool.get());
    writer.WriteImage(0, &albedoInfo)
        .WriteImage(1, &normalInfo)
        .WriteImage(2, &matInfo)
        .WriteImage(3, &posInfo)
        .WriteImage(4, &depthInfo)
        .Overwrite(GetCurrentFrame().gbuffer.imageSet);

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
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D(0, 0), m_screenImageExtent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;

    cmd.beginRendering(&renderingInfo);
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
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D(0, 0), m_screenImageExtent);
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
    vk::ClearValue clearValue{};
    clearValue.depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    depthAttachment.imageView = m_depthImage.imageView;
    depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue = clearValue;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D(0, 0), m_screenImageExtent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 0;
    renderingInfo.pColorAttachments = nullptr;
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;

    cmd.beginRendering(&renderingInfo);
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
    n8          padding[12];
};

static_assert(sizeof(OcclusionObjectData) == 192 && "Size mismatch");

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
    auto& visibilityResultsBuffer = GetCurrentFrame().visiblityResultBuffer;
    auto& rendererDataBuffer = GetCurrentFrame().rendererDataBuffer;

    objectDataBuffer.reset();
    visibilityResultsBuffer.reset();
    rendererDataBuffer.reset();

    objectDataBuffer = std::make_unique<Buffer>(&m_logicalDevice, objectDataForGPU.size() * sizeof(OcclusionObjectData), 1,
                                                vk::BufferUsageFlagBits::eStorageBuffer,
                                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                                VMA_MEMORY_USAGE_AUTO, 1, "occlusion object data buffer");

    visibilityResultsBuffer =
        std::make_unique<Buffer>(&m_logicalDevice, objectDataForGPU.size() * sizeof(VisiblityResultSet), 1, vk::BufferUsageFlagBits::eStorageBuffer,
                                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO, 1,
                                 "occlusion visibility results buffer");

    rendererDataBuffer = std::make_unique<Buffer>(&m_logicalDevice, sizeof(RendererData), 1, vk::BufferUsageFlagBits::eUniformBuffer,
                                                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                                  VMA_MEMORY_USAGE_AUTO, 1, "renderer data buffer");

    RendererData renderDataContent{{m_swapChain->GetExtent().width, m_swapChain->GetExtent().height}};

    vk::MemoryBarrier2 memoryBarrier{};
    memoryBarrier.srcStageMask = vk::PipelineStageFlagBits2::eLateFragmentTests;
    memoryBarrier.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    memoryBarrier.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
    memoryBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;

    vk::DependencyInfo dependencyInfo{};
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;
    cmd.pipelineBarrier2(&dependencyInfo);

    Utils::ImageTransitionInfo preComputeReadTransition{
        cmd,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        &m_logicalDevice,
        m_depthImage.image,
        vk::ImageAspectFlagBits::eDepth,
    };
    Utils::TransitionImageLayout(preComputeReadTransition);

    objectDataBuffer->Map();
    objectDataBuffer->WriteToBuffer((void*)objectDataForGPU.data());
    objectDataBuffer->UnMap();

    rendererDataBuffer->Map();
    rendererDataBuffer->WriteToBuffer((void*)&renderDataContent);
    rendererDataBuffer->UnMap();

    vk::DescriptorImageInfo  depthInfo = {m_depthImageSampler, m_depthImage.imageView, vk::ImageLayout::eShaderReadOnlyOptimal};
    vk::DescriptorBufferInfo boundingBoxGpuInfo = objectDataBuffer->DescriptorInfo();
    vk::DescriptorBufferInfo visiblityGpuInfo = {visibilityResultsBuffer->GetBuffer(), 0, VK_WHOLE_SIZE};
    vk::DescriptorBufferInfo projectionGpuInfo = cam.GetCombinedDataBufferHandle(m_currentFrameIndex).DescriptorInfo();
    vk::DescriptorBufferInfo rendererDataGpuInfo = rendererDataBuffer->DescriptorInfo();

    vk::DescriptorSet& computeSet = GetCurrentFrame().computeSet;

    size_t actualSize = visibilityResultsBuffer->GetBufferSize();
    size_t bindRange = sizeof(VisiblityResultSet) * objectDataForGPU.size();

    DescriptorWriter writer(*m_computeLayout, m_computePool.get());
    writer.WriteImage(0, &depthInfo)
        .WriteBuffer(1, &boundingBoxGpuInfo)
        .WriteBuffer(2, &visiblityGpuInfo)
        .WriteBuffer(3, &projectionGpuInfo)
        .WriteBuffer(4, &rendererDataGpuInfo);

    if(computeSet == VK_NULL_HANDLE) { writer.Build(computeSet); }
    else { writer.Overwrite(computeSet); }

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_computePipelineLayout, 0, 1, &computeSet, 0, nullptr);

    n32 size = objectDataForGPU.size();
    cmd.pushConstants(m_computePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(n32), &size);

    uint32_t groupCountX = (static_cast<uint32_t>(objectDataForGPU.size()) + 63) / 64;
    if(groupCountX > 0) { cmd.dispatch(groupCountX, 1, 1); }

    WaitForCompute(cmd);

    Utils::ImageTransitionInfo postComputeDepthTransition{cmd,
                                                          vk::ImageLayout::eShaderReadOnlyOptimal,
                                                          vk::ImageLayout::eDepthAttachmentOptimal,
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
