#include "renderer.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "extra.hpp"
#include "images.hpp"
#include "logger.hpp"
#include "scene_handler.hpp"

#include <array>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

namespace Humongous
{

Renderer::Renderer(Window& window, const LogicalDevice& logicalDevice, const PhysicalDevice& physicalDevice, ResourceManager& resourceManager,
                   VmaAllocator allocator, vk::Format drawFormat, vk::Format depthFormat)
    : m_window{window}, m_logicalDevice{logicalDevice}, m_resourceManager{resourceManager}, m_physicalDevice{physicalDevice}, m_allocator{allocator}
{
    CreateCommandPool();
    CreateCommandBuffers();
    CreateComputePipeline();
    CreateSyncStructures();
    CreateLightingPipeline();
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

        frame.gbuffer.albedo.Destroy(m_logicalDevice);
        frame.gbuffer.normalRough.Destroy(m_logicalDevice);
        frame.gbuffer.materialParam.Destroy(m_logicalDevice);
        frame.gbuffer.depth.Destroy(m_logicalDevice);

        frame.visiblityResultBuffer.reset();
        frame.debugBuffer.reset();
        frame.objectDataBuffer.reset();
        frame.rendererDataBuffer.reset();

        frame.drawImage.Destroy(m_logicalDevice);

        frame.hiZImage.Destroy(m_logicalDevice);
        for(auto& mip: frame.hiZMips)
        {
            m_logicalDevice.GetVkDevice().destroyImageView(mip.sampledView);
            m_logicalDevice.GetVkDevice().destroyImageView(mip.storageView);
        }
    }

    if(m_depthImageSampler != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroySampler(m_depthImageSampler); }
    if(m_debugImageSampler != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroySampler(m_debugImageSampler); }

    m_occlusionPipeline.reset();
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(m_occlusionPipelineLayout, nullptr);

    m_mipPipeline.reset();
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(m_mipPipelineLayout, nullptr);

    m_lightingPipeline.reset();
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(m_lightingPipelineLayout, nullptr);

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
        m_swapChain = std::make_unique<SwapChain>(m_window, m_physicalDevice, m_logicalDevice, std::move(m_swapChain));
    }
    // recreate the image views

    HGINFO("Recreated swap chain");

    m_logicalDevice.GetVkDevice().waitIdle();

    m_screenImageExtent = m_swapChain->GetExtent();
    GetCurrentFrame().drawImage.imageExtent = vk::Extent3D(m_screenImageExtent.width, m_screenImageExtent.height, 0.0);

    CreateDrawImage();
    CreateGBuffer();
}

void Renderer::CreateDrawImage()
{
    HGINFO("Creating draw image and view...");

    vk::Extent3D drawImageExtent = {m_swapChain->GetExtent().width, m_swapChain->GetExtent().height, 1};

    vk::ImageUsageFlags drawImageUsages{};
    drawImageUsages |= vk::ImageUsageFlagBits::eTransferSrc;
    drawImageUsages |= vk::ImageUsageFlagBits::eTransferDst;
    drawImageUsages |= vk::ImageUsageFlagBits::eStorage;
    drawImageUsages |= vk::ImageUsageFlagBits::eColorAttachment;

    Utils::AllocatedImageCreateInfo imgCI{.logicalDevice = m_logicalDevice};
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
    imgCI.format = vk::Format::eR16G16B16A16Sfloat;
    imgCI.imagePool = VK_NULL_HANDLE;
    imgCI.samples = vk::SampleCountFlagBits::e1;

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();
    for(auto& frame: m_frames)
    {
        if(frame.drawImage.imageView != VK_NULL_HANDLE) { vkDestroyImageView(m_logicalDevice.GetVkDevice(), frame.drawImage.imageView, nullptr); }
        if(frame.drawImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, frame.drawImage.image, frame.drawImage.allocation); }

        imgCI.allocatedImage = &frame.drawImage;
        Utils::CreateAllocatedImage(imgCI);

        Utils::ImageTransitionInfo transInfo{.logicalDevice = m_logicalDevice};
        transInfo.image = frame.drawImage.image;
        transInfo.oldLayout = vk::ImageLayout::eUndefined;
        transInfo.newLayout = vk::ImageLayout::eGeneral;
        transInfo.cmd = cmd;
        transInfo.imageAspect = vk::ImageAspectFlagBits::eColor;
        Utils::TransitionImageLayout(transInfo);

        frame.drawImage.imageLayout = vk::ImageLayout::eGeneral;
    }

    m_logicalDevice.EndSingleTimeCommands(cmd);

    HGINFO("Created draw image and view");
}

void Renderer::CreateGBuffer()
{
    HGINFO("Initializing G-Buffer...");

    vk::ImageUsageFlags colorUsages = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

    vk::SamplerReductionModeCreateInfo reductionInfo{};
    reductionInfo.reductionMode = vk::SamplerReductionMode::eMin;

    Utils::SamplerCreateInfo samCI{};
    samCI.mipMode = vk::SamplerMipmapMode::eNearest;
    samCI.magFilter = vk::Filter::eLinear;
    samCI.minFilter = vk::Filter::eLinear;
    samCI.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samCI.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samCI.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samCI.pNext = &reductionInfo;

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
        builder.AddPoolSize(vk::DescriptorType::eStorageImage, 45);
        builder.SetMaxSets(50);
        m_lightingPool = builder.Build();
    }
    else { m_lightingPool->ResetPool(); }

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();
    u32  mipLevels = floor(log2(std::max(imgCI.width, imgCI.height))) + 1;

    for(int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
    {
        if(m_frames[i].gbuffer.albedo.image != VK_NULL_HANDLE) { m_frames[i].gbuffer.albedo.Destroy(m_logicalDevice); }
        if(m_frames[i].gbuffer.normalRough.image != VK_NULL_HANDLE) { m_frames[i].gbuffer.normalRough.Destroy(m_logicalDevice); }
        if(m_frames[i].gbuffer.materialParam.image != VK_NULL_HANDLE) { m_frames[i].gbuffer.materialParam.Destroy(m_logicalDevice); }
        if(m_frames[i].gbuffer.depth.image != VK_NULL_HANDLE) { m_frames[i].gbuffer.depth.Destroy(m_logicalDevice); }
        if(m_frames[i].hiZImage.image != VK_NULL_HANDLE) { m_frames[i].hiZImage.Destroy(m_logicalDevice); }

        imgCI.allocatedImage = &m_frames[i].gbuffer.albedo;
        imgCI.usage = colorUsages;
        imgCI.format = vk::Format::eR8G8B8A8Unorm;
        imgCI.aspectFlags = vk::ImageAspectFlagBits::eColor;
        imgCI.initialLayout = vk::ImageLayout::eUndefined;
        imgCI.mipLevels = 1;
        Utils::CreateAllocatedImage(imgCI);

        {
            Utils::ImageTransitionInfo trans{.logicalDevice = m_logicalDevice};
            trans.cmd = cmd;
            trans.image = m_frames[i].gbuffer.albedo.image;
            trans.oldLayout = vk::ImageLayout::eUndefined;
            trans.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            trans.imageAspect = vk::ImageAspectFlagBits::eColor;
            Utils::TransitionImageLayout(trans);
            m_frames[i].gbuffer.albedo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        }

        imgCI.allocatedImage = &m_frames[i].gbuffer.normalRough;
        Utils::CreateAllocatedImage(imgCI);
        {
            Utils::ImageTransitionInfo trans{.logicalDevice = m_logicalDevice};
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
            Utils::ImageTransitionInfo trans{.logicalDevice = m_logicalDevice};
            trans.cmd = cmd;
            trans.image = m_frames[i].gbuffer.materialParam.image;
            trans.oldLayout = vk::ImageLayout::eUndefined;
            trans.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            trans.imageAspect = vk::ImageAspectFlagBits::eColor;
            Utils::TransitionImageLayout(trans);
            m_frames[i].gbuffer.materialParam.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        }

        imgCI.allocatedImage = &m_frames[i].gbuffer.depth;
        imgCI.aspectFlags = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        imgCI.format = vk::Format::eD32SfloatS8Uint;

        vk::ImageUsageFlags depthImageUsages{};
        depthImageUsages |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
        depthImageUsages |= vk::ImageUsageFlagBits::eSampled;
        depthImageUsages |= vk::ImageUsageFlagBits::eTransferSrc;
        imgCI.usage = depthImageUsages;
        imgCI.initialLayout = vk::ImageLayout::eUndefined;
        Utils::CreateAllocatedImage(imgCI);
        {
            Utils::ImageTransitionInfo trans{.logicalDevice = m_logicalDevice};
            trans.cmd = cmd;
            trans.image = m_frames[i].gbuffer.depth.image;
            trans.oldLayout = vk::ImageLayout::eUndefined;
            trans.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            trans.imageAspect = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
            Utils::TransitionImageLayout(trans);
            m_frames[i].gbuffer.depth.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        }

        imgCI.format = vk::Format::eR32Sfloat;
        imgCI.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
        imgCI.allocatedImage = &m_frames[i].hiZImage;
        imgCI.aspectFlags = vk::ImageAspectFlagBits::eColor;
        imgCI.mipLevels = mipLevels;
        Utils::CreateAllocatedImage(imgCI);
        {
            Utils::ImageTransitionInfo trans{.logicalDevice = m_logicalDevice};
            trans.cmd = cmd;
            trans.image = m_frames[i].hiZImage.image;
            trans.oldLayout = vk::ImageLayout::eUndefined;
            trans.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            trans.imageAspect = vk::ImageAspectFlagBits::eColor;
            Utils::TransitionImageLayout(trans);
            m_frames[i].hiZImage.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        }

        for(auto& mip: m_frames[i].hiZMips)
        {
            m_logicalDevice.GetVkDevice().destroyImageView(mip.sampledView);
            m_logicalDevice.GetVkDevice().destroyImageView(mip.storageView);
        }

        m_frames[i].hiZMips.resize(imgCI.mipLevels);
        for(u32 level = 0; level < imgCI.mipLevels; ++level)
        {
            Utils::ImageTransitionInfo transition{.cmd = cmd, .logicalDevice = m_logicalDevice, .image = m_frames[i].hiZImage.image};
            transition.oldLayout = vk::ImageLayout::eUndefined;
            transition.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            transition.imageAspect = vk::ImageAspectFlagBits::eColor;
            transition.baseMipLevel = level;
            transition.levelCount = 1;
            transition.baseArrayLayer = 0;
            transition.layerCount = 1;
            Utils::TransitionImageLayout(transition);

            vk::ImageViewCreateInfo viewInfo{};
            viewInfo.image = m_frames[i].hiZImage.image;
            viewInfo.format = vk::Format::eR32Sfloat;
            viewInfo.viewType = vk::ImageViewType::e2D;
            viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseMipLevel = level;

            if(m_logicalDevice.GetVkDevice().createImageView(&viewInfo, nullptr, &m_frames[i].hiZMips[level].sampledView) != vk::Result::eSuccess)
            {
                HGFATAL("Failed to create sampled depthMip");
            }
            if(m_logicalDevice.GetVkDevice().createImageView(&viewInfo, nullptr, &m_frames[i].hiZMips[level].storageView) != vk::Result::eSuccess)
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

    m_logicalDevice.EndSingleTimeCommands(cmd);

    HGINFO("Initialized G-Buffer (resized to %d x %d)", m_screenImageExtent.width, m_screenImageExtent.height);
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

    if(m_logicalDevice.GetVkDevice().createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_lightingPipelineLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create lighting layout");
    }

    HGINFO("Created lighting layout");

    HGINFO("Creating pipeline...");

    ComputePipeline::ComputePipelineCreateInfo configInfo{.logicalDevice = m_logicalDevice};
    configInfo.pipelineLayout = m_lightingPipelineLayout;
    configInfo.shaderFile = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "lighting.comp");

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
    }

    HGINFO("Created command pool");
}

void Renderer::CreateCommandBuffers()
{
    HGINFO("Allocating command buffers...");

    m_frames.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

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

        ComputePipeline::ComputePipelineCreateInfo configInfo{.logicalDevice = m_logicalDevice};
        configInfo.pipelineLayout = m_occlusionPipelineLayout;
        configInfo.shaderFile = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "occlusion.comp");

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

        ComputePipeline::ComputePipelineCreateInfo configInfo{.logicalDevice = m_logicalDevice};
        configInfo.pipelineLayout = m_mipPipelineLayout;
        configInfo.shaderFile = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "mip.comp");

        m_mipPipeline = std::make_unique<ComputePipeline>(configInfo);
    }
    HGINFO("Created compute pipeline");
}

void Renderer::ReadyPerFrameData(std::vector<Utils::VisibleEntityInfo>& visibleEntities)
{
    auto world = SceneHandler::GetWorld();

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

void Renderer::PreGeometryPassTransitions(vk::CommandBuffer cmd)
{
    auto& currentFrame = GetCurrentFrame();

    Utils::ImageTransitionInfo drawInfo{.logicalDevice = m_logicalDevice};
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
        drawInfo.image = currentFrame.gbuffer.depth.image;
        drawInfo.oldLayout = currentFrame.gbuffer.depth.imageLayout;
        drawInfo.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        drawInfo.imageAspect = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        currentFrame.gbuffer.depth.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
}

void Renderer::PostGeometryPassTransitions(vk::CommandBuffer cmd)
{
    auto& currentFrame = GetCurrentFrame();

    Utils::ImageTransitionInfo drawInfo{.logicalDevice = m_logicalDevice};
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
        drawInfo.image = currentFrame.gbuffer.depth.image;
        drawInfo.oldLayout = currentFrame.gbuffer.depth.imageLayout;
        drawInfo.imageAspect = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        currentFrame.gbuffer.depth.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        Utils::TransitionImageLayout(drawInfo);
    }
}

vk::CommandBuffer Renderer::BeginFrame(std::vector<Utils::VisibleEntityInfo>& visibleEntities)
{
    vk::Result result = m_logicalDevice.GetVkDevice().waitForFences(1, &GetCurrentFrame().inFlightFence, vk::True, std::numeric_limits<u64>::max());
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

    if(GetCurrentFrame().drawImage.imageLayout == vk::ImageLayout::eTransferSrcOptimal)
    {
        auto                       cmd = m_logicalDevice.BeginSingleTimeCommands();
        Utils::ImageTransitionInfo transInfo{.logicalDevice = m_logicalDevice};
        transInfo.image = GetCurrentFrame().drawImage.image;
        transInfo.oldLayout = GetCurrentFrame().drawImage.imageLayout;
        transInfo.newLayout = vk::ImageLayout::eGeneral;
        transInfo.cmd = cmd;
        transInfo.imageAspect = vk::ImageAspectFlagBits::eColor;
        Utils::TransitionImageLayout(transInfo);
        GetCurrentFrame().drawImage.imageLayout = vk::ImageLayout::eGeneral;
        m_logicalDevice.EndSingleTimeCommands(cmd);
    }

    m_screenImageExtent = m_swapChain->GetExtent();

    GetCurrentFrame().drawImage.imageExtent = vk::Extent3D(m_screenImageExtent, 0.0);

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

    Utils::ImageTransitionInfo drawInfo{.logicalDevice = m_logicalDevice};
    drawInfo.image = GetCurrentFrame().drawImage.image;
    drawInfo.oldLayout = GetCurrentFrame().drawImage.imageLayout;
    drawInfo.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    drawInfo.cmd = cmd;
    Utils::TransitionImageLayout(drawInfo);

    GetCurrentFrame().drawImage.imageLayout = vk::ImageLayout::eTransferSrcOptimal;

    Utils::ImageTransitionInfo swapInfo{.logicalDevice = m_logicalDevice};
    swapInfo.image = m_swapChain->GetImages()[m_currentImageIndex];
    swapInfo.oldLayout = vk::ImageLayout::eUndefined;
    swapInfo.newLayout = vk::ImageLayout::eTransferDstOptimal;
    swapInfo.cmd = cmd;

    Utils::TransitionImageLayout(swapInfo);

    Utils::CopyImageToImage(cmd, GetCurrentFrame().drawImage.image, m_swapChain->GetImages()[m_currentImageIndex], m_screenImageExtent,
                            m_swapChain->GetExtent());

    Utils::ImageTransitionInfo presentTransitionInfo{.logicalDevice = m_logicalDevice};
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
    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {0.5f, 0.5f, 0.5f, 0.5f};
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(0.0f, 0);

    vk::RenderingAttachmentInfo attach{};
    attach.sType = vk::StructureType::eRenderingAttachmentInfo;
    attach.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attach.loadOp = vk::AttachmentLoadOp::eClear;
    attach.storeOp = vk::AttachmentStoreOp::eStore;
    attach.clearValue = clearValues[0];

    auto& currentFrame = GetCurrentFrame();

    std::array<vk::RenderingAttachmentInfo, 3> colorAttachments{attach, attach, attach};
    colorAttachments[0].imageView = currentFrame.gbuffer.albedo.imageView;
    colorAttachments[1].imageView = currentFrame.gbuffer.normalRough.imageView;
    colorAttachments[2].imageView = currentFrame.gbuffer.materialParam.imageView;

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    depthAttachment.imageView = currentFrame.gbuffer.depth.imageView;
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue = clearValues[1];

    vk::RenderingAttachmentInfo stencilAttachment{};
    stencilAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    stencilAttachment.imageView = currentFrame.gbuffer.depth.imageView;
    stencilAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    stencilAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    stencilAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    stencilAttachment.clearValue = clearValues[1];

    vk::RenderingInfo renderingInfo{};
    renderingInfo.sType = vk::StructureType::eRenderingInfo;
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D(0, 0), m_screenImageExtent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = &stencilAttachment;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;

    cmd.beginRendering(&renderingInfo);
}

void Renderer::EndGeometryPass(vk::CommandBuffer cmd)
{
    cmd.endRendering();
    PostGeometryPassTransitions(cmd);
}

void Renderer::DoLightingPass(vk::CommandBuffer cmd, vk::DescriptorSet camSet, vk::DescriptorSet sceneSet, vk::DescriptorSet skyboxSet)
{
    auto& currentFrame = GetCurrentFrame();
    auto  albedoInfo = currentFrame.gbuffer.albedo.GetDescriptorInfo();
    auto  normalInfo = currentFrame.gbuffer.normalRough.GetDescriptorInfo();
    auto  matInfo = currentFrame.gbuffer.materialParam.GetDescriptorInfo();
    auto  depthInfo = currentFrame.gbuffer.depth.GetDescriptorInfo();
    auto  drawInfo = currentFrame.drawImage.GetDescriptorInfo();

    DescriptorWriter writer(*m_lightingDescriptorLayout, m_lightingPool.get());
    writer.WriteImage(0, &albedoInfo)
        .WriteImage(1, &normalInfo)
        .WriteImage(2, &matInfo)
        .WriteImage(3, &depthInfo)
        .WriteImage(4, &drawInfo)
        .Overwrite(currentFrame.gbuffer.imageSet);

    m_lightingPipeline->BindPipeline(cmd);

    std::vector<vk::DescriptorSet> sets = {camSet, sceneSet, skyboxSet, currentFrame.gbuffer.imageSet};
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_lightingPipelineLayout, 0, sets.size(), sets.data(), 0, nullptr);

    u32 localSize = 8;

    u32 countX = std::ceil(m_screenImageExtent.width / localSize);
    u32 countY = std::ceil(m_screenImageExtent.height / localSize);

    cmd.dispatch(countX, countY, 1);

    WaitForCompute(cmd);

    Utils::ImageTransitionInfo transitionInfo{.logicalDevice = m_logicalDevice};
    transitionInfo.image = GetCurrentFrame().drawImage.image;
    transitionInfo.oldLayout = GetCurrentFrame().drawImage.imageLayout;
    transitionInfo.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    transitionInfo.cmd = cmd;
    Utils::TransitionImageLayout(transitionInfo);

    GetCurrentFrame().drawImage.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
}

void Renderer::BeginSkyboxPass(vk::CommandBuffer cmd)
{
    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(0.0f, 0);

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    colorAttachment.imageView = GetCurrentFrame().drawImage.imageView;
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = clearValues[0];

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    depthAttachment.imageView = GetCurrentFrame().gbuffer.depth.imageView;
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.clearValue = clearValues[1];

    vk::RenderingInfo renderingInfo{};
    renderingInfo.sType = vk::StructureType::eRenderingInfo;
    renderingInfo.renderArea = vk::Rect2D(vk::Offset2D(0, 0), m_screenImageExtent);
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

void Renderer::BeginUIRendering(vk::CommandBuffer cmd)
{
    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    colorAttachment.imageView = GetCurrentFrame().drawImage.imageView;
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
    PreGeometryPassTransitions(cmd);

    vk::ClearValue clearValue{};
    clearValue.depthStencil = vk::ClearDepthStencilValue{0.0f, 0};

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    depthAttachment.imageView = GetCurrentFrame().gbuffer.depth.imageView;
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
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

    cmd.beginRendering(&renderingInfo);
}

void Renderer::EndDepthPrePass(vk::CommandBuffer cmd) { vkCmdEndRendering(cmd); }

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

void Renderer::DoOcclusionCulling(vk::CommandBuffer cmd, const std::vector<Utils::VisibleEntityInfo>& frustumCulledEntities, World& world,
                                  const Camera& cam)
{
    auto& currentFrame = GetCurrentFrame();
    m_mipPipeline->BindPipeline(cmd);

    {
        Utils::ImageTransitionInfo transSrcInfo{cmd,
                                                currentFrame.gbuffer.depth.imageLayout,
                                                vk::ImageLayout::eShaderReadOnlyOptimal,
                                                m_logicalDevice,
                                                currentFrame.gbuffer.depth.image,
                                                vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
                                                0,
                                                1,
                                                0,
                                                1};
        Utils::TransitionImageLayout(transSrcInfo);
        currentFrame.gbuffer.depth.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    {
        Utils::ImageTransitionInfo mip0DestTransition{.logicalDevice = m_logicalDevice};
        mip0DestTransition.cmd = cmd;
        mip0DestTransition.oldLayout = currentFrame.hiZMips[0].layout;
        mip0DestTransition.newLayout = vk::ImageLayout::eGeneral;
        mip0DestTransition.image = currentFrame.hiZImage.image;
        mip0DestTransition.baseMipLevel = 0;
        mip0DestTransition.imageAspect = vk::ImageAspectFlagBits::eColor;
        mip0DestTransition.levelCount = 1;
        mip0DestTransition.baseArrayLayer = 0;
        mip0DestTransition.layerCount = 1;
        Utils::TransitionImageLayout(mip0DestTransition);
        currentFrame.hiZMips[0].layout = vk::ImageLayout::eGeneral;

        DescriptorWriter        writer_mip0(*m_mipDescriptorLayout, m_computePool.get());
        vk::DescriptorImageInfo gBufferSourceInfo(m_depthImageSampler, currentFrame.gbuffer.depth.imageView,
                                                  vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo hiZ_Mip0_DestInfo({}, currentFrame.hiZMips[0].storageView, vk::ImageLayout::eGeneral);

        writer_mip0.WriteImage(0, &gBufferSourceInfo).WriteImage(1, &hiZ_Mip0_DestInfo).Overwrite(currentFrame.hiZMips[0].set);

        u32 level = 0;

        cmd.pushConstants(m_mipPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(u32), &level);

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_mipPipelineLayout, 0, 1, &currentFrame.hiZMips[0].set, 0, nullptr);

        u32 initialMipWidth = currentFrame.hiZImage.width;
        u32 initialMipHeight = currentFrame.hiZImage.height;
        cmd.dispatch((initialMipWidth + 7) / 8, (initialMipHeight + 7) / 8, 1);

        Utils::ImageTransitionInfo mip0DestTranstition2{.logicalDevice = m_logicalDevice};
        mip0DestTranstition2.cmd = cmd;
        mip0DestTranstition2.oldLayout = currentFrame.hiZMips[0].layout;
        mip0DestTranstition2.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        mip0DestTranstition2.image = currentFrame.hiZImage.image;
        mip0DestTranstition2.baseMipLevel = 0;
        mip0DestTranstition2.imageAspect = vk::ImageAspectFlagBits::eColor;
        mip0DestTranstition2.levelCount = 1;
        mip0DestTranstition2.baseArrayLayer = 0;
        mip0DestTranstition2.layerCount = 1;
        Utils::TransitionImageLayout(mip0DestTranstition2);
        currentFrame.hiZMips[0].layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    for(u32 i = 0; i < currentFrame.hiZImage.mipLevels - 1; ++i)
    {
        Utils::ImageTransitionInfo srcTransition{.logicalDevice = m_logicalDevice};
        srcTransition.cmd = cmd;
        srcTransition.oldLayout = currentFrame.hiZMips[i].layout;
        srcTransition.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        srcTransition.image = currentFrame.hiZImage.image;
        srcTransition.baseMipLevel = i;
        srcTransition.imageAspect = vk::ImageAspectFlagBits::eColor;
        srcTransition.levelCount = 1;
        srcTransition.baseArrayLayer = 0;
        srcTransition.layerCount = 1;
        Utils::TransitionImageLayout(srcTransition);

        currentFrame.hiZMips[i].layout = vk::ImageLayout::eShaderReadOnlyOptimal;

        Utils::ImageTransitionInfo destTransition{.logicalDevice = m_logicalDevice};
        destTransition.cmd = cmd;
        destTransition.oldLayout = currentFrame.hiZMips[i + 1].layout;
        destTransition.newLayout = vk::ImageLayout::eGeneral;
        destTransition.image = currentFrame.hiZImage.image;
        destTransition.baseMipLevel = i + 1;
        destTransition.imageAspect = vk::ImageAspectFlagBits::eColor;
        destTransition.levelCount = 1;
        destTransition.baseArrayLayer = 0;
        destTransition.layerCount = 1;

        Utils::TransitionImageLayout(destTransition);

        currentFrame.hiZMips[i + 1].layout = vk::ImageLayout::eGeneral;

        vk::ImageView sourceMipView = currentFrame.hiZMips[i].sampledView;
        vk::ImageView destMipView = currentFrame.hiZMips[i + 1].storageView;

        DescriptorWriter        writer(*m_mipDescriptorLayout, m_computePool.get());
        vk::DescriptorImageInfo sourceImageInfo;
        if(i == 0)
        {
            sourceImageInfo =
                vk::DescriptorImageInfo(m_depthImageSampler, currentFrame.gbuffer.depth.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
        else { sourceImageInfo = vk::DescriptorImageInfo(m_depthImageSampler, sourceMipView, vk::ImageLayout::eShaderReadOnlyOptimal); }
        vk::DescriptorImageInfo destImageInfo({}, destMipView, vk::ImageLayout::eGeneral);

        writer.WriteImage(0, &sourceImageInfo).WriteImage(1, &destImageInfo).Overwrite(currentFrame.hiZMips[i + 1].set);

        cmd.pushConstants(m_mipPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(u32), &i + 1);

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_mipPipelineLayout, 0, 1, &currentFrame.hiZMips[i + 1].set, 0, nullptr);

        u32 destMipWidth = std::max(1u, currentFrame.hiZImage.width >> (i + 1));
        u32 destMipHeight = std::max(1u, currentFrame.hiZImage.height >> (i + 1));

        cmd.dispatch((destMipWidth + 7) / 8, (destMipHeight + 7) / 8, 1);

        Utils::ImageTransitionInfo sourceNextIterTransition{.logicalDevice = m_logicalDevice};
        sourceNextIterTransition.cmd = cmd;
        sourceNextIterTransition.oldLayout = currentFrame.hiZMips[i + 1].layout;
        sourceNextIterTransition.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        sourceNextIterTransition.image = currentFrame.hiZImage.image;
        sourceNextIterTransition.baseMipLevel = i + 1;
        sourceNextIterTransition.imageAspect = vk::ImageAspectFlagBits::eColor;
        sourceNextIterTransition.levelCount = 1;
        sourceNextIterTransition.baseArrayLayer = 0;
        sourceNextIterTransition.layerCount = 1;
        Utils::TransitionImageLayout(sourceNextIterTransition);
        currentFrame.hiZMips[i + 1].layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }
    currentFrame.hiZImage.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    WaitForCompute(cmd);

    m_occlusionPipeline->BindPipeline(cmd);

    std::vector<OcclusionObjectData> objectDataForGPU;
    objectDataForGPU.reserve(frustumCulledEntities.size());

    currentFrame.numObjectsDispatched = 0;

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
        else { HGWARN("Invalid bounding box?"); }
    }

    if(objectDataForGPU.empty()) { return; }
    // HGDEBUG("We're trying to occlusion cull %i objects", objectDataForGPU.size());

    currentFrame.numObjectsDispatched = objectDataForGPU.size();

    auto& objectDataBuffer = currentFrame.objectDataBuffer;
    auto& visibilityResultsBuffer = currentFrame.visiblityResultBuffer;
    auto& rendererDataBuffer = currentFrame.rendererDataBuffer;

    objectDataBuffer.reset();
    visibilityResultsBuffer.reset();
    rendererDataBuffer.reset();

    objectDataBuffer =
        std::make_unique<Buffer>(m_logicalDevice, objectDataForGPU.size() * sizeof(OcclusionObjectData), 1, vk::BufferUsageFlagBits::eStorageBuffer,
                                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO, 1,
                                 "occlusion object data buffer");

    visibilityResultsBuffer =
        std::make_unique<Buffer>(m_logicalDevice, objectDataForGPU.size() * sizeof(VisiblityResultSet), 1, vk::BufferUsageFlagBits::eStorageBuffer,
                                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO, 1,
                                 "occlusion visibility results buffer");

    rendererDataBuffer = std::make_unique<Buffer>(m_logicalDevice, sizeof(RendererData), 1, vk::BufferUsageFlagBits::eUniformBuffer,
                                                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                                  VMA_MEMORY_USAGE_AUTO, 1, "occlusion renderer data buffer");

    RendererData renderDataContent{{m_swapChain->GetExtent().width, m_swapChain->GetExtent().height}};

    objectDataBuffer->Map();
    objectDataBuffer->WriteToBuffer((void*)objectDataForGPU.data());
    objectDataBuffer->UnMap();

    rendererDataBuffer->Map();
    rendererDataBuffer->WriteToBuffer((void*)&renderDataContent);
    rendererDataBuffer->UnMap();

    vk::DescriptorImageInfo  depthInfo = {m_depthImageSampler, currentFrame.hiZImage.imageView, vk::ImageLayout::eShaderReadOnlyOptimal};
    vk::DescriptorBufferInfo boundingBoxGpuInfo = objectDataBuffer->DescriptorInfo();
    vk::DescriptorBufferInfo visiblityGpuInfo = visibilityResultsBuffer->DescriptorInfo();
    vk::DescriptorBufferInfo projectionGpuInfo = cam.GetCombinedDataBufferHandle(m_currentFrameIndex).DescriptorInfo();
    vk::DescriptorBufferInfo rendererDataGpuInfo = rendererDataBuffer->DescriptorInfo();

    vk::DescriptorSet& computeSet = currentFrame.occlusionSet;

    DescriptorWriter writer(*m_occlusionDescriptorLayout, m_computePool.get());
    writer.WriteImage(0, &depthInfo)
        .WriteBuffer(1, &boundingBoxGpuInfo)
        .WriteBuffer(2, &visiblityGpuInfo)
        .WriteBuffer(3, &projectionGpuInfo)
        .WriteBuffer(4, &rendererDataGpuInfo)
        .Overwrite(computeSet);

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_occlusionPipelineLayout, 0, 1, &computeSet, 0, nullptr);

    u32 size = objectDataForGPU.size();
    cmd.pushConstants(m_occlusionPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(u32), &size);

    u32 groupCountX = (size + 63) / 64;
    if(groupCountX > 0) { cmd.dispatch(groupCountX, 1, 1); }

    WaitForCompute(cmd);

    {
        Utils::ImageTransitionInfo transSrcInfo{cmd,
                                                currentFrame.gbuffer.depth.imageLayout,
                                                vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                                m_logicalDevice,
                                                currentFrame.gbuffer.depth.image,
                                                vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
                                                0,
                                                1,
                                                0,
                                                1};
        Utils::TransitionImageLayout(transSrcInfo);
        currentFrame.gbuffer.depth.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    }
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
