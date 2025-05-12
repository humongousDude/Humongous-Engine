#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "extra.hpp"
#include "gameobject.hpp"
#include "images.hpp"
#include "logger.hpp"
#include <array>
#include <renderer.hpp>
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

    RecreateSwapChain();
    CreateCommandPool();
    AllocateCommandBuffers();
    InitSyncStructures();
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
    }

    if(m_drawImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, m_drawImage.image, m_drawImage.allocation); }
    if(m_depthImage.imageView != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroyImageView(m_drawImage.imageView, nullptr); }
    if(m_depthImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, m_depthImage.image, m_depthImage.allocation); }
    if(m_depthImage.imageView != VK_NULL_HANDLE) { m_logicalDevice.GetVkDevice().destroyImageView(m_depthImage.imageView, nullptr); }

    m_logicalDevice.GetVkDevice().destroyPipeline(m_computePipeline, nullptr);
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(m_computePipelineLayout, nullptr);

    m_computeLayout.reset();
    m_computePool.reset();

    vkDestroySampler(m_logicalDevice.GetVkDevice(), m_depthImageSampler, nullptr);

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

    Utils::AllocatedImageCreateInfo imgCI{.logicalDevice = m_logicalDevice, .allocatedImage = m_drawImage};
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

    Utils::AllocatedImageCreateInfo imgCI{.logicalDevice = m_logicalDevice, .allocatedImage = m_depthImage};
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
    DescriptorPool::Builder builder{m_logicalDevice};
    builder.AddPoolSize(vk::DescriptorType::eCombinedImageSampler, 5);
    builder.AddPoolSize(vk::DescriptorType::eStorageBuffer, 5);
    builder.AddPoolSize(vk::DescriptorType::eUniformBuffer, 5);
    builder.AddPoolSize(vk::DescriptorType::eStorageImage, 5);
    builder.SetMaxSets(100);
    m_computePool = builder.Build();

    DescriptorSetLayout::Builder builder2{m_logicalDevice};
    builder2.addBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    builder2.addBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.addBinding(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.addBinding(3, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.addBinding(4, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute);
    builder2.addBinding(5, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute);
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
}

void Renderer::ReadyPerFrameData(std::vector<std::pair<n32, class GameObject*>>* gameObjects)
{
    auto&    visibilityResultsBuffer = GetCurrentFrame().visibilityResults;
    uint32_t numObjectsCulledLastFrame = GetCurrentFrame().numObjectsDispatched;

    if(!visibilityResultsBuffer || numObjectsCulledLastFrame == 0) { return; }

    if(visibilityResultsBuffer->GetBufferSize() < numObjectsCulledLastFrame * sizeof(VisiblityResultSet))
    {
        HGERROR("Visibility results buffer size mismatch for frame %u! Expected at least %zu bytes, buffer is %zu.", m_currentFrameIndex,
                numObjectsCulledLastFrame * sizeof(VisiblityResultSet), visibilityResultsBuffer->GetBufferSize());
        return;
    }

    visibilityResultsBuffer->Map();
    VisiblityResultSet* results = static_cast<VisiblityResultSet*>(visibilityResultsBuffer->GetMappedMemory());

    std::unordered_map<uint32_t, bool> prevFrameVisibilityById;
    prevFrameVisibilityById.reserve(numObjectsCulledLastFrame);

    for(uint32_t i = 0; i < numObjectsCulledLastFrame; ++i)
    {
        uint32_t objectId = results[i].id;
        bool     isVisible = results[i].visible;

        prevFrameVisibilityById[objectId] = isVisible;
    }

    visibilityResultsBuffer->UnMap();

    std::vector<std::pair<n32, GameObject*>> visibleObjectsForThisFrame;
    visibleObjectsForThisFrame.reserve(gameObjects->size());

    for(auto& pair: *gameObjects)
    {
        uint32_t    objectId = pair.first;
        GameObject* object = pair.second;

        auto it = prevFrameVisibilityById.find(objectId);

        bool isVisibleThisFrame = false;

        if(it != prevFrameVisibilityById.end()) { isVisibleThisFrame = it->second; }

        if(isVisibleThisFrame) { visibleObjectsForThisFrame.push_back(pair); }
    }

    *gameObjects = std::move(visibleObjectsForThisFrame);
}

vk::CommandBuffer Renderer::BeginFrame(std::vector<std::pair<n32, class GameObject*>>* gameobjects)
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
        return nullptr;
    }

    if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        HGERROR("failed to acquire swap chain image!");
        return nullptr;
    }

    ReadyPerFrameData(gameobjects);

    vk::CommandBuffer cmd = GetCurrentFrame().commandBuffer;
    cmd.reset();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

    if(cmd.begin(&beginInfo) != vk::Result::eSuccess) { HGERROR("Failed to begin recording command buffer"); }

    return cmd;
}

void Renderer::EndFrame()
{
    GetCurrentFrame().commandBuffer.end();

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

void Renderer::BeginRendering(vk::CommandBuffer cmd)
{
    m_drawImageExtent.width = m_drawImage.imageExtent.width;
    m_drawImageExtent.height = m_drawImage.imageExtent.height;
    m_depthImageExtent.width = m_depthImage.imageExtent.width;
    m_depthImageExtent.height = m_depthImage.imageExtent.height;

    Utils::ImageTransitionInfo transInfo{};
    transInfo.image = m_drawImage.image;
    transInfo.oldLayout = vk::ImageLayout::eUndefined;
    transInfo.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    transInfo.cmd = cmd;

    Utils::TransitionImageLayout(transInfo);

    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color = {0.3f, 0.3f, 0.3f, 1.0f};
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView = m_drawImage.imageView;
    colorAttachment.imageLayout = vk::ImageLayout::eGeneral;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.clearValue = clearValues[0];

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.imageView = m_depthImage.imageView;
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachment.clearValue = clearValues[1];

    vk::RenderingInfo renderingInfo{};
    // renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {0, 0, m_swapChain->GetExtent().width, m_swapChain->GetExtent().height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;
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
}

void Renderer::EndRendering(vk::CommandBuffer cmd)
{
    vkCmdEndRendering(cmd);

    Utils::ImageTransitionInfo drawInfo{};
    drawInfo.image = m_drawImage.image;
    drawInfo.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    drawInfo.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    drawInfo.cmd = cmd;

    Utils::ImageTransitionInfo swapInfo{};
    swapInfo.image = m_swapChain->GetImages()[m_currentImageIndex];
    swapInfo.oldLayout = vk::ImageLayout::eUndefined;
    swapInfo.newLayout = vk::ImageLayout::eTransferDstOptimal;
    swapInfo.cmd = cmd;

    Utils::TransitionImageLayout(drawInfo);
    Utils::TransitionImageLayout(swapInfo);

    Utils::CopyImageToImage(cmd, m_drawImage.image, m_swapChain->GetImages()[m_currentImageIndex], m_drawImageExtent, m_swapChain->GetExtent());

    Utils::ImageTransitionInfo presentInfo{};
    presentInfo.image = m_swapChain->GetImages()[m_currentImageIndex];
    presentInfo.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    presentInfo.newLayout = vk::ImageLayout::ePresentSrcKHR;
    presentInfo.cmd = cmd;

    Utils::TransitionImageLayout(presentInfo);
}

void Renderer::BeginDepthPrePass(vk::CommandBuffer cmd)
{
    m_drawImageExtent.width = m_drawImage.imageExtent.width;
    m_drawImageExtent.height = m_drawImage.imageExtent.height;
    m_depthImageExtent.width = m_depthImage.imageExtent.width;
    m_depthImageExtent.height = m_depthImage.imageExtent.height;

    vk::ClearValue clearValue{};
    clearValue.depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    vk::RenderingAttachmentInfo depthAttachment{};
    // depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
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
    float       padding_id[3]; // 3 * 4 = 12 bytes padding
};

void Renderer::DoGPUOcclusionCulling(vk::CommandBuffer cmd, std::vector<std::pair<n32, class GameObject*>>* gameObjects, const Camera& cam)
{
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_computePipeline);

    std::vector<OcclusionObjectData> objectData{};
    GetCurrentFrame().numObjectsDispatched = 0;

    for(auto& [id, object]: *gameObjects)
    {
        OcclusionObjectData data;
        data.boundingBox = (object->GetBoundingBox());
        data.id = id;

        objectData.push_back(data);
    }
    if(objectData.empty()) { return; }

    GetCurrentFrame().numObjectsDispatched = objectData.size();

    auto& objectDataBuffer = GetCurrentFrame().objectDataBuffer;
    auto& visibilityResultsBuffer = GetCurrentFrame().visibilityResults;
    auto& rendererDataBuffer = GetCurrentFrame().rendererDataBuffer;

    objectDataBuffer.reset();
    visibilityResultsBuffer.reset();
    rendererDataBuffer.reset();

    objectDataBuffer = std::make_unique<Buffer>();
    visibilityResultsBuffer = std::make_unique<Buffer>();
    rendererDataBuffer = std::make_unique<Buffer>();

    objectDataBuffer->Init(&m_logicalDevice, objectData.size() * sizeof(OcclusionObjectData), 1, vk::BufferUsageFlagBits::eStorageBuffer,
                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO);

    visibilityResultsBuffer->Init(&m_logicalDevice, objectData.size() * sizeof(VisiblityResultSet), 1, vk::BufferUsageFlagBits::eStorageBuffer,
                                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO);

    rendererDataBuffer->Init(&m_logicalDevice, sizeof(RendererData), 1, vk::BufferUsageFlagBits::eUniformBuffer,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_AUTO);

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
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        &m_logicalDevice,
        m_depthImage.image,
        vk::ImageAspectFlagBits::eDepth,
    };

    Utils::TransitionImageLayout(preComputeReadTransition);

    objectDataBuffer->Map();
    objectDataBuffer->WriteToBuffer((void*)objectData.data());
    objectDataBuffer->UnMap();

    RendererData renderData{{m_swapChain->GetExtent().width, m_swapChain->GetExtent().height}};
    rendererDataBuffer->Map();
    rendererDataBuffer->WriteToBuffer((void*)&renderData);
    rendererDataBuffer->UnMap();

    vk::DescriptorImageInfo  depthInfo = {m_depthImageSampler, m_depthImage.imageView, vk::ImageLayout::eShaderReadOnlyOptimal};
    vk::DescriptorBufferInfo boundingBoxInfo = objectDataBuffer->DescriptorInfo();
    vk::DescriptorBufferInfo visiblityInfo = visibilityResultsBuffer->DescriptorInfo();
    vk::DescriptorBufferInfo projectionInfo = cam.GetCombinedDataBufferHandle(m_currentFrameIndex).DescriptorInfo();
    vk::DescriptorBufferInfo rendererDataInfo = rendererDataBuffer->DescriptorInfo();

    vk::DescriptorSet& computeSet = GetCurrentFrame().computeSet;

    if(computeSet == VK_NULL_HANDLE)
    {
        DescriptorWriter(*m_computeLayout, m_computePool.get())
            .WriteImage(0, &depthInfo)
            .WriteBuffer(1, &boundingBoxInfo)
            .WriteBuffer(2, &visiblityInfo)
            .WriteBuffer(3, &projectionInfo)
            .WriteBuffer(4, &rendererDataInfo)
            .Build(computeSet);
    }
    else
    {
        DescriptorWriter(*m_computeLayout, m_computePool.get())
            .WriteImage(0, &depthInfo)
            .WriteBuffer(1, &boundingBoxInfo)
            .WriteBuffer(2, &visiblityInfo)
            .WriteBuffer(3, &projectionInfo)
            .WriteBuffer(4, &rendererDataInfo)
            .Overwrite(computeSet);
    }
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_computePipelineLayout, 0, 1, &computeSet, 0, nullptr);

    vkCmdDispatch(cmd, (n32)(gameObjects->size() + 63) / 64, 1, 1);

    WaitForCompute(cmd);

    Utils::ImageTransitionInfo postComputeDepthTransition{cmd,
                                                          vk::ImageLayout::eShaderReadOnlyOptimal,
                                                          vk::ImageLayout::eDepthStencilAttachmentOptimal,
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
